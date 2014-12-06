#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/version.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
//#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
//#include <linux/config.h>
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/info.h>
#include <sound/pcm_params.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/dmapool.h>
#include <linux/delay.h>

#include "i2sio.h"
#include "ar7240.h"
#include "ar7240_i2s.h"		/* local definitions */
#include "ath79-pcm.h"
#include "ar71xx_regs.h"
#include "ath79.h"

/* Playback Default Stereo - Fl/Fr */
static int p_chmask = 0x3;
module_param(p_chmask, uint, S_IRUGO);
MODULE_PARM_DESC(p_chmask, "Playback Channel Mask");

/* Playback Default 44.1 KHz */
static int p_srate = 44100;
module_param(p_srate, uint, S_IRUGO);
MODULE_PARM_DESC(p_srate, "Playback Sampling Rate");

/* Playback Default 16bits/sample */
static int p_ssize = 2;
module_param(p_ssize, uint, S_IRUGO);
MODULE_PARM_DESC(p_ssize, "Playback Sample Size(bytes)");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31))
#       define ar7240_dma_cache_sync(b, c)              \
        do {                                            \
                dma_cache_sync(NULL, (void *)b,         \
                                c, DMA_FROM_DEVICE);    \
        } while (0)
#       define ar7240_cache_inv(d, s)                   \
        do {                                            \
                dma_sync_single(NULL, (dma_addr_t)d, s, \
                                DMA_TO_DEVICE);         \
        } while (0)
#       define AR7240_IRQF_DISABLED     IRQF_DISABLED
#else
#       define ar7240_dma_cache_sync(b, c)              \
        do {                                            \
                dma_cache_wback((unsigned long)b, c);   \
        } while (0)
#       define ar7240_cache_inv(d, s)                   \
        do {                                            \
                dma_cache_inv((unsigned long)d, s);     \
        } while (0)
#       define AR7240_IRQF_DISABLED     SA_INTERRUPT
#endif

/* Keep the below in sync with WLAN driver code
   parameters */
#define AOW_PER_DESC_INTERVAL 125      /* In usec */
#define DESC_FREE_WAIT_BUFFER 200      /* In usec */


int ar7240_i2s_major = 253;
int ar7240_i2s_minor = 0;
spinlock_t ath79_pcm_lock;
static struct dma_pool *ath79_pcm_cache;
spinlock_t ath79_stereo_lock;

module_param(ar7240_i2s_major, int, S_IRUGO);
module_param(ar7240_i2s_minor, int, S_IRUGO);

MODULE_AUTHOR("Jacob@Atheros");
MODULE_LICENSE("Dual BSD/GPL");

void ar7240_i2sound_i2slink_on(int master);
void ar7240_i2sound_request_dma_channel(int);
void ar7240_i2sound_dma_desc(unsigned long, int);
void ar7240_i2sound_dma_start(int);
void ar7240_i2sound_dma_pause(int);
void ar7240_i2sound_dma_resume(int);
void ar7240_i2s_clk(unsigned long, unsigned long);
//irqreturn_t ar7240_i2s_intr(int irq, void *dev_id, struct pt_regs *regs);
irqreturn_t ar7240_i2s_intr(int irq, void *dev_id);

#define TRUE    1
#define FALSE   !(TRUE)

#define AR7240_STEREO_CLK_DIV (AR7240_STEREO_BASE+0x1c)
/*
 * XXX : This is the interface between i2s and wlan
 *       When adding info, here please make sure that
 *       it is reflected in the wlan side also
 */
typedef struct i2s_stats {
    unsigned int write_fail;
    unsigned int rx_underflow;
    unsigned int aow_sync_enabled;
    unsigned int sync_buf_full;
    unsigned int sync_buf_empty;
    unsigned int tasklet_count;
    unsigned int repeat_count;
} i2s_stats_t;

i2s_stats_t stats;

/*
 * Functions and data structures relating to the Audio Sync feature
 *
 */

/* general defines */

typedef struct i2s_buf {
	uint8_t *bf_vaddr;
	unsigned long bf_paddr;
} i2s_buf_t;


typedef struct i2s_dma_buf {
	ar7240_mbox_dma_desc *lastbuf;
	ar7240_mbox_dma_desc *db_desc;
	dma_addr_t db_desc_p;
	i2s_buf_t db_buf[NUM_DESC];
	int tail;
} i2s_dma_buf_t;

typedef struct ar7240_i2s_softc {
	int ropened;
	int popened;
	i2s_dma_buf_t sc_pbuf;
	i2s_dma_buf_t sc_rbuf;
	char *sc_pmall_buf;
	char *sc_rmall_buf;
	int sc_irq;
	int ft_value;
	int ppause;
	int rpause;
	spinlock_t i2s_lock;   /* lock */
	unsigned long i2s_lockflags;
    struct class *i2sclass; /* /dev/i2s */
} ar7240_i2s_softc_t;

typedef struct snd_ar9331 {
    struct snd_card *card;
    struct snd_pcm *pcm;
	struct snd_pcm_substream *playback;
	struct snd_pcm_substream *capture;

}snd_ar9331_t;

snd_ar9331_t snd_chip;
ar7240_i2s_softc_t sc_buf_var;
i2s_stats_t stats;

void i2s_get_stats(i2s_stats_t *p)
{
    memcpy(p, &stats, sizeof(struct i2s_stats));
}EXPORT_SYMBOL(i2s_get_stats);

void i2s_clear_stats(void)
{
   stats.write_fail = 0;
   stats.rx_underflow = 0;
   stats.sync_buf_full = 0;
   stats.sync_buf_empty = 0;
   stats.tasklet_count = 0;
   stats.repeat_count = 0;
}EXPORT_SYMBOL(i2s_clear_stats);

enum wmdac_type {        /* keep sorted in alphabetical order */
	wm8918,
};
static const struct i2c_device_id wm8918_ids[] = {
    { "wm8918", wm8918, }, //该i2c_driver所支持的i2c_client
    { /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, wm8918_ids); //来告知用户空间, 模块支持那些设备
static const unsigned short normal_i2c[] = {0x1A, I2C_CLIENT_END };
struct i2c_client *wm8918_i2c_client = NULL;
//低8位和高8位互换
static u16 wm8918_swap(u16 data)
{
	u16 tmp = (data << 8) & 0xff00;
	tmp = tmp | ((data >> 8) & 0x00ff);
	return tmp;
}
static int wm8918_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
//	int ret;
	wm8918_i2c_client = client;
//	printk(KERN_CRIT "probe, i2c_clist*:%p\n", client);
//	printk(KERN_CRIT "i2c_clist->addr:%x\n", client->addr);
//	printk(KERN_CRIT "i2c_clist->flag:%u\n", client->flags);

	//i2c_smbus_write_word_data()小端传输,先b7-b0,后b15-b8. wm8918是大端
    i2c_smbus_write_word_data(client, 0x00, wm8918_swap(0x0000));
    i2c_smbus_write_word_data(client, 0x16, wm8918_swap(0x0006));
    i2c_smbus_write_word_data(client, 0x6C, wm8918_swap(0x0100));
    i2c_smbus_write_word_data(client, 0x6F, wm8918_swap(0x0100));
	mdelay(500); //延迟一会,否则设置失败.至少延迟300ms,设置250ms失败,设置为300ms成功
    i2c_smbus_write_word_data(client, 0x14, wm8918_swap(0x845E));
    i2c_smbus_write_word_data(client, 0x39, wm8918_swap(0x0039));
    i2c_smbus_write_word_data(client, 0x3A, wm8918_swap(0x00B9));
    i2c_smbus_write_word_data(client, 0x21, wm8918_swap(0x0000));
    i2c_smbus_write_word_data(client, 0x68, wm8918_swap(0x0005));
    i2c_smbus_write_word_data(client, 0x19, wm8918_swap(0x0002));
//    i2c_smbus_write_word_data(client, 0x1E, wm8918_swap(0x01C0));
//    i2c_smbus_write_word_data(client, 0x1F, wm8918_swap(0x01C0));
#if 0
	mdelay(1000);
	ret = i2c_smbus_read_word_data(client, 0x00);
	printk(KERN_CRIT "read %04x from 0x00\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x16);
	printk(KERN_CRIT "read %04x from 0x16\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x6C);
	printk(KERN_CRIT "read %04x from 0x6C\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x6F);
	printk(KERN_CRIT "read %04x from 0x6F\n", wm8918_swap(ret));


	ret = i2c_smbus_read_word_data(client, 0x14);
	printk(KERN_CRIT "read %04x from 0x14\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x39);
	printk(KERN_CRIT "read %04x from 0x39\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x3A);
	printk(KERN_CRIT "read %04x from 0x3A\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x21);
	printk(KERN_CRIT "read %04x from 0x21\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x68);
	printk(KERN_CRIT "read %04x from 0x68\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x18);
	printk(KERN_CRIT "read %04x from 0x18\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x19);
	printk(KERN_CRIT "read %04x from 0x19\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x1A);
	printk(KERN_CRIT "read %04x from 0x1A\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x1B);
	printk(KERN_CRIT "read %04x from 0x1B\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x1E);
	printk(KERN_CRIT "read %04x from 0x1E\n", wm8918_swap(ret));

	ret = i2c_smbus_read_word_data(client, 0x1F);
	printk(KERN_CRIT "read %04x from 0x1F\n", wm8918_swap(ret));
#endif
	return 0;
}
static int wm8918_remove(struct i2c_client *client)
{
//	printk(KERN_CRIT "remove, i2c_clist*:%p\n", client);
	return 0;
}
static int wm8918_detect(struct i2c_client *new_client, struct i2c_board_info *info)
{
	return 0;
}
static struct i2c_driver wm8918_driver = {
    .class        = I2C_CLASS_HWMON,
    .driver = {
        .name     = "wm8918",
    },
    .probe        = wm8918_probe, //probe设备探测函数,i2c_add_driver()时会被调用 
    .remove       = wm8918_remove,
    .id_table     = wm8918_ids, //id_table成员用于设置当前i2c_driver所支持的设备类型
    .detect       = wm8918_detect,
    .address_list = normal_i2c,
};

void ath79_mbox_dma_prepare(struct ath79_pcm_rt_priv *rtpriv)
{
	struct ath79_pcm_desc *desc;
//	u32 t;

	ar7240_reg_wr(MBOX_FIFO_RESET, 0x05); // virian
	udelay(1000);
    printk("ath79_mbox_dma_prepare %d\n", rtpriv->direction);

	if (rtpriv->direction == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Request the DMA channel to the controller */
//		t = ath79_dma_rr(AR934X_DMA_REG_MBOX_DMA_POLICY);
//		ath79_dma_wr(AR934X_DMA_REG_MBOX_DMA_POLICY,
//			     t | AR934X_DMA_MBOX_DMA_POLICY_RX_QUANTUM |
//			     (6 << AR934X_DMA_MBOX_DMA_POLICY_TX_FIFO_THRESH_SHIFT));
        ar7240_reg_wr(MBOX_DMA_POLICY, 0x4a);
	    udelay(500);

		/* The direction is indicated from the DMA engine perspective
		 * i.e. we'll be using the RX registers for Playback and
		 * the TX registers for capture */
		desc = list_first_entry(&rtpriv->dma_head, struct ath79_pcm_desc, list);
//		ath79_dma_wr(AR934X_DMA_REG_MBOX0_DMA_RX_DESCRIPTOR_BASE,
//				(u32) desc->phys);

		ar7240_reg_wr(MBOX0_DMA_RX_DESCRIPTOR_BASE, desc->phys);
	    udelay(500);

//		ath79_mbox_interrupt_enable(AR934X_DMA_MBOX0_INT_RX_COMPLETE);
        ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_RX_DMA_COMPLETE);
	    udelay(500);
	} else {
		/* Request the DMA channel to the controller */
//		t = ath79_dma_rr(AR934X_DMA_REG_MBOX_DMA_POLICY);
//		ath79_dma_wr(AR934X_DMA_REG_MBOX_DMA_POLICY,
//			     t | AR934X_DMA_MBOX_DMA_POLICY_TX_QUANTUM |
//			     (6 << AR934X_DMA_MBOX_DMA_POLICY_TX_FIFO_THRESH_SHIFT));
        ar7240_reg_wr(MBOX_DMA_POLICY, 0x6a);

		desc = list_first_entry(&rtpriv->dma_head, struct ath79_pcm_desc, list);
//		ath79_dma_wr(AR934X_DMA_REG_MBOX0_DMA_TX_DESCRIPTOR_BASE,
//				(u32) desc->phys);
		ar7240_reg_wr(MBOX0_DMA_TX_DESCRIPTOR_BASE, desc->phys);
//		ath79_mbox_interrupt_enable(AR934X_DMA_MBOX0_INT_TX_COMPLETE);
        ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_TX_DMA_COMPLETE);

	}
}

int ath79_mbox_dma_map(struct ath79_pcm_rt_priv *rtpriv, dma_addr_t baseaddr,
			      int period_bytes,int bufsize)
{
	struct list_head *head = &rtpriv->dma_head;
	struct ath79_pcm_desc *desc, *prev;
	dma_addr_t desc_p;
	unsigned int offset = 0;

	spin_lock(&ath79_pcm_lock);

	rtpriv->elapsed_size = 0;
	/* We loop until we have enough buffers to map the requested DMA area */
	do {
		/* Allocate a descriptor and insert it into the DMA ring */
		desc = dma_pool_alloc(ath79_pcm_cache, GFP_KERNEL, &desc_p);
		if(!desc) {
			return -ENOMEM;
		}
		memset(desc, 0, sizeof(struct ath79_pcm_desc));
		desc->phys = desc_p;
		list_add_tail(&desc->list, head);

		desc->OWN = 1;
		desc->rsvd1 = desc->rsvd2 = desc->rsvd3 = desc->EOM = 0;

		/* buffer size may not be a multiple of period_bytes */
		if (bufsize >= offset + period_bytes) {
			desc->size = period_bytes;
		} else {
			desc->size = bufsize - offset;
		}
		desc->BufPtr = baseaddr + offset;

		/* For now, we assume the buffer is always full
		 * -->length == size */
		desc->length = desc->size;

		/* We need to make sure we are not the first descriptor.
		 * If we are, prev doesn't point to a struct ath79_pcm_desc */
		if (desc->list.prev != head) {
			prev =
			    list_entry(desc->list.prev, struct ath79_pcm_desc,
				       list);
			prev->NextPtr = desc->phys;
		}

		offset += desc->size;
	} while (offset < bufsize);

	/* Once all the descriptors have been created, we can close the loop
	 * by pointing from the last one to the first one */
	desc = list_first_entry(head, struct ath79_pcm_desc, list);
	prev = list_entry(head->prev, struct ath79_pcm_desc, list);
	prev->NextPtr = desc->phys;

	spin_unlock(&ath79_pcm_lock);

	return 0;
}

void ath79_mbox_dma_unmap(struct ath79_pcm_rt_priv *rtpriv)
{
	struct list_head *head = &rtpriv->dma_head;
	struct ath79_pcm_desc *desc, *n;

	spin_lock(&ath79_pcm_lock);
	list_for_each_entry_safe(desc, n, head, list) {
		list_del(&desc->list);
		dma_pool_free(ath79_pcm_cache, desc, desc->phys);
	}
	spin_unlock(&ath79_pcm_lock);

	return;
}

int ath79_mbox_dma_init(struct device *dev)
{
	int ret = 0;

	/* Allocate a DMA pool to store the MBOX descriptor */
	ath79_pcm_cache = dma_pool_create("ath79_pcm_pool", dev,
					 sizeof(struct ath79_pcm_desc), 4, 0);
	if (!ath79_pcm_cache)
		ret = -ENOMEM;

	return ret;
}

void ath79_mbox_dma_exit(void)
{
	dma_pool_destroy(ath79_pcm_cache);
	ath79_pcm_cache = NULL;
}

int ar7240_i2s_init(struct file *filp)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	i2s_buf_t *scbuf;
	uint8_t *bufp = NULL;
//	int j, k, byte_cnt, tail = 0, mode = 1;
	int j, byte_cnt, tail = 0, mode = 1;
	ar7240_mbox_dma_desc *desc;
	unsigned long desc_p;

        if (!filp) {
            mode = FMODE_WRITE;
        } else {
            mode = filp->f_mode;
        }

	if (mode & FMODE_READ) {
			dmabuf = &sc->sc_rbuf;
			sc->ropened = 1;
			sc->rpause = 0;
		} else {
			dmabuf = &sc->sc_pbuf;
			sc->popened = 1;
			sc->ppause = 0;
		}

		dmabuf->db_desc = (ar7240_mbox_dma_desc *)
		    dma_alloc_coherent(NULL,
				       NUM_DESC *
				       sizeof(ar7240_mbox_dma_desc),
				       &dmabuf->db_desc_p, GFP_DMA);

		if (dmabuf->db_desc == NULL) {
			printk(KERN_CRIT "DMA desc alloc failed for %d\n",
		       mode);
			return ENOMEM;
		}

		for (j = 0; j < NUM_DESC; j++) {
			dmabuf->db_desc[j].OWN = 0;

		}

		/* Allocate data buffers */
		scbuf = dmabuf->db_buf;

		if (!(bufp = kmalloc(NUM_DESC * I2S_BUF_SIZE, GFP_KERNEL))) {
			printk(KERN_CRIT
			       "Buffer allocation failed for \n");
			goto fail3;
		}

	if (mode & FMODE_READ)
			sc->sc_rmall_buf = bufp;
		else
			sc->sc_pmall_buf = bufp;

		for (j = 0; j < NUM_DESC; j++) {
			scbuf[j].bf_vaddr = &bufp[j * I2S_BUF_SIZE];
			scbuf[j].bf_paddr =
			    dma_map_single(NULL, scbuf[j].bf_vaddr,
					   I2S_BUF_SIZE,
					   DMA_BIDIRECTIONAL);

		}
		dmabuf->tail = 0;

		// Initialize desc
			desc = dmabuf->db_desc;
			desc_p = (unsigned long) dmabuf->db_desc_p;
			byte_cnt = NUM_DESC * I2S_BUF_SIZE;
			tail = dmabuf->tail;

			while (byte_cnt && (tail < NUM_DESC)) {
				desc[tail].rsvd1 = 0;
				desc[tail].size = I2S_BUF_SIZE;
				if (byte_cnt > I2S_BUF_SIZE) {
					desc[tail].length = I2S_BUF_SIZE;
					byte_cnt -= I2S_BUF_SIZE;
					desc[tail].EOM = 0;
				} else {
					desc[tail].length = byte_cnt;
					byte_cnt = 0;
					desc[tail].EOM = 0;
				}
				desc[tail].rsvd2 = 0;
				desc[tail].rsvd3 = 0;
				desc[tail].BufPtr =
				    (unsigned int) scbuf[tail].bf_paddr;
				desc[tail].NextPtr =
				    (desc_p +
				     ((tail +
				       1) *
			      (sizeof(ar7240_mbox_dma_desc))));
		if (mode & FMODE_READ) {
					desc[tail].OWN = 1;
				} else {
					desc[tail].OWN = 0;
				}
				tail++;
			}
			tail--;
			desc[tail].NextPtr = desc_p;

		dmabuf->tail = 0;

	return 0;

fail3:
	if (mode & FMODE_READ)
			dmabuf = &sc->sc_rbuf;
		else
			dmabuf = &sc->sc_pbuf;
		dma_free_coherent(NULL,
				  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
				  dmabuf->db_desc, dmabuf->db_desc_p);
	if (mode & FMODE_READ) {
			if (sc->sc_rmall_buf)
				kfree(sc->sc_rmall_buf);
		} else {
			if (sc->sc_pmall_buf)
				kfree(sc->sc_pmall_buf);
		}

	return -ENOMEM;
}

int ar7240_i2s_init_for_pcm(struct snd_pcm_substream *substream)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	i2s_buf_t *scbuf;
	uint8_t *bufp = NULL;
//	int j, k, byte_cnt, tail = 0, mode = 1;
	int j, byte_cnt, tail = 0;
	ar7240_mbox_dma_desc *desc;
	unsigned long desc_p;

//        if (!filp) {
//            mode = FMODE_WRITE;
//        } else {
//            mode = filp->f_mode;
//        }
//
//	if (mode & FMODE_READ) {
//			dmabuf = &sc->sc_rbuf;
//			sc->ropened = 1;
//			sc->rpause = 0;
//		} else {
//			dmabuf = &sc->sc_pbuf;
//			sc->popened = 1;
//			sc->ppause = 0;
//		}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			dmabuf = &sc->sc_rbuf;
			sc->ropened = 1;
			sc->rpause = 0;
		} else {
			dmabuf = &sc->sc_pbuf;
			sc->popened = 1;
			sc->ppause = 0;
		}

		dmabuf->db_desc = (ar7240_mbox_dma_desc *)
		    dma_alloc_coherent(NULL,
				       NUM_DESC *
				       sizeof(ar7240_mbox_dma_desc),
				       &dmabuf->db_desc_p, GFP_DMA);

		if (dmabuf->db_desc == NULL) {
			printk(KERN_CRIT "DMA desc alloc failed for \n");
			return ENOMEM;
		}

		for (j = 0; j < NUM_DESC; j++) {
			dmabuf->db_desc[j].OWN = 0;

		}

		/* Allocate data buffers */
		scbuf = dmabuf->db_buf;

		if (!(bufp = kmalloc(NUM_DESC * I2S_BUF_SIZE, GFP_KERNEL))) {
			printk(KERN_CRIT
			       "Buffer allocation failed for \n");
			goto fail3;
		}

	//if (mode & FMODE_READ)
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			sc->sc_rmall_buf = bufp;
		else
			sc->sc_pmall_buf = bufp;

		for (j = 0; j < NUM_DESC; j++) {
			scbuf[j].bf_vaddr = &bufp[j * I2S_BUF_SIZE];
			scbuf[j].bf_paddr =
			    dma_map_single(NULL, scbuf[j].bf_vaddr,
					   I2S_BUF_SIZE,
					   DMA_BIDIRECTIONAL);

		}
		dmabuf->tail = 0;

		// Initialize desc
			desc = dmabuf->db_desc;
			desc_p = (unsigned long) dmabuf->db_desc_p;
			byte_cnt = NUM_DESC * I2S_BUF_SIZE;
			tail = dmabuf->tail;

			while (byte_cnt && (tail < NUM_DESC)) {
				desc[tail].rsvd1 = 0;
				desc[tail].size = I2S_BUF_SIZE;
				if (byte_cnt > I2S_BUF_SIZE) {
					desc[tail].length = I2S_BUF_SIZE;
					byte_cnt -= I2S_BUF_SIZE;
					desc[tail].EOM = 0;
				} else {
					desc[tail].length = byte_cnt;
					byte_cnt = 0;
					desc[tail].EOM = 0;
				}
				desc[tail].rsvd2 = 0;
				desc[tail].rsvd3 = 0;
				desc[tail].BufPtr =
				    (unsigned int) scbuf[tail].bf_paddr;
				desc[tail].NextPtr =
				    (desc_p +
				     ((tail +
				       1) *
			      (sizeof(ar7240_mbox_dma_desc))));
//		if (mode & FMODE_READ) {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
					desc[tail].OWN = 1;
				} else {
					desc[tail].OWN = 0;
				}
				tail++;
			}
			tail--;
			desc[tail].NextPtr = desc_p;

		dmabuf->tail = 0;

	return 0;

fail3:
//	if (mode & FMODE_READ)
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			dmabuf = &sc->sc_rbuf;
		else
			dmabuf = &sc->sc_pbuf;
		dma_free_coherent(NULL,
				  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
				  dmabuf->db_desc, dmabuf->db_desc_p);
//	if (mode & FMODE_READ) {
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (sc->sc_rmall_buf)
				kfree(sc->sc_rmall_buf);
		} else {
			if (sc->sc_pmall_buf)
				kfree(sc->sc_pmall_buf);
		}

	return -ENOMEM;
}
int ar7240_i2s_open(struct inode *inode, struct file *filp)
{

	ar7240_i2s_softc_t *sc = &sc_buf_var;
	int opened = 0, mode = MASTER;

	if ((filp->f_mode & FMODE_READ) && (sc->ropened)) {
        printk("%s, %d I2S mic busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if ((filp->f_mode & FMODE_WRITE) && (sc->popened)) {
        printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	opened = (sc->ropened | sc->popened);

	/* Reset MBOX FIFO's */
	if (!opened) {
//		ar7240_reg_wr(MBOX_FIFO_RESET, 0xff); // virian
		ar7240_reg_wr(MBOX_FIFO_RESET, 0x05); // virian
		udelay(500);
	}

	/* Allocate and initialize descriptors */
	if (ar7240_i2s_init(filp) == ENOMEM)
		return -ENOMEM;

	//printk("Set default to 48K,16bits,stereo\n");

	printk(KERN_CRIT "opened:%d\n", opened);
	if (!opened) {
	    ar7240_i2sound_request_dma_channel(mode);
    }

	return (0);

}

int ar7240_i2s_open_for_pcm(struct snd_pcm_substream *substream)
{

	ar7240_i2s_softc_t *sc = &sc_buf_var;
	int opened = 0, mode = MASTER;

//	if ((filp->f_mode & FMODE_READ) && (sc->ropened)) {
//        printk("%s, %d I2S mic busy\n", __func__, __LINE__);
//		return -EBUSY;
//	}
//	if ((filp->f_mode & FMODE_WRITE) && (sc->popened)) {
//        printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
//		return -EBUSY;
//	}
	if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE) && (sc->ropened)) {
        printk("%s, %d I2S mic busy\n", __func__, __LINE__);
		return -EBUSY;
	}
	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) && (sc->popened)) {
        printk("%s, %d I2S speaker busy\n", __func__, __LINE__);
		return -EBUSY;
	}

	opened = (sc->ropened | sc->popened);

	/* Reset MBOX FIFO's */
	if (!opened) {
//		ar7240_reg_wr(MBOX_FIFO_RESET, 0xff); // virian
		ar7240_reg_wr(MBOX_FIFO_RESET, 0x05); // virian
		udelay(500);
	}

	/* Allocate and initialize descriptors */
	if (ar7240_i2s_init_for_pcm(substream) == ENOMEM)
		return -ENOMEM;

	//printk("Set default to 48K,16bits,stereo\n");

	printk(KERN_CRIT "opened:%d\n", opened);
	if (!opened) {
	    ar7240_i2sound_request_dma_channel(mode);
    }

	return (0);

}

ssize_t ar7240_i2s_read(struct file * filp, char __user * buf,
			 size_t count, loff_t * f_pos)
{
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_rbuf;
	i2s_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	unsigned int byte_cnt, mode = 1, offset = 0, tail = dmabuf->tail;
	unsigned long desc_p;
	int need_start = 0;
//	static unsigned long lastJ =0, byteCnt= 0;

	byte_cnt = count;

	if (sc->ropened < 2) {
		ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_TX_DMA_COMPLETE);
		need_start = 1;
	}

	sc->ropened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

	while (byte_cnt && !desc[tail].OWN) {
		if (byte_cnt >= I2S_BUF_SIZE) {
			desc[tail].length = I2S_BUF_SIZE;
			byte_cnt -= I2S_BUF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
			byte_cnt = 0;
		}
//		byteCnt += I2S_BUF_SIZE;
//		if(jiffies - lastJ > HZ)
//		{
//			lastJ = jiffies;
//			printk("ReadRate=%lu\n", byteCnt);
//			byteCnt = 0;
//		}
        dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_FROM_DEVICE);
		desc[tail].rsvd2 = 0;
		retval = copy_to_user(buf + offset, scbuf[tail].bf_vaddr, I2S_BUF_SIZE);
		if (retval)
			return retval;
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].OWN = 1;

		tail = next_tail(tail);
		offset += I2S_BUF_SIZE;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_i2sound_dma_desc((unsigned long) desc_p, mode);
        if (filp) {
		    ar7240_i2sound_dma_start(mode);
        }
	} else if (!sc->rpause) {
		ar7240_i2sound_dma_resume(mode);
	}

	return offset;
}

ssize_t ar7240_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos, int resume)
{
	/* 循环队列 */
#define prev_tail(t) ({ (t == 0) ? (NUM_DESC - 1) : (t - 1); })
#define next_tail(t) ({ (t == (NUM_DESC - 1)) ? 0 : (t + 1); })

	uint8_t *data;
	ssize_t retval;
	int byte_cnt, offset, need_start = 0;
	int mode = 0;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf = &sc->sc_pbuf;
	i2s_buf_t *scbuf;
	ar7240_mbox_dma_desc *desc;
	int tail = dmabuf->tail;
	unsigned long desc_p;
    int data_len = 0;
//		int k = 0;
//	static unsigned long lastJ = 0;
//	static unsigned long currentTotalByte = 0;

	I2S_LOCK(sc);

	byte_cnt = count;

	if (sc->popened < 2) {
        ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_RX_DMA_COMPLETE | RX_UNDERFLOW);
		need_start = 1;
	}

	sc->popened = 2;

	scbuf = dmabuf->db_buf;
	desc = dmabuf->db_desc;
	desc_p = (unsigned long) dmabuf->db_desc_p;
	offset = 0;
	data = scbuf[0].bf_vaddr;

	desc_p += tail * sizeof(ar7240_mbox_dma_desc);

	while (byte_cnt && !desc[tail].OWN) {
        if (byte_cnt >= I2S_BUF_SIZE) {
			desc[tail].length = I2S_BUF_SIZE;
			byte_cnt -= I2S_BUF_SIZE;
            data_len = I2S_BUF_SIZE;
		} else {
			desc[tail].length = byte_cnt;
            data_len = byte_cnt;
			byte_cnt = 0;
		}
//		currentTotalByte += desc[tail].length;
//		if(jiffies - lastJ > HZ)
//		{
//			printk("Rate=%lu\n", currentTotalByte);
//			currentTotalByte = 0;
//			lastJ = jiffies;
//		}
        if(!filp)
        {
            memcpy(scbuf[tail].bf_vaddr, buf + offset, data_len);
        }
        else
        {
            retval =
                copy_from_user(scbuf[tail].bf_vaddr, buf + offset, data_len);
            if (retval)
                return retval;
        }
//		for(k = 0; k < data_len; k++)
//		{
//			printk(KERN_CRIT" %02x\n", (scbuf[tail].bf_vaddr)[k]);
//		}

		/*对由dma_alloc_noncoherent()申请的内存做局部映射,其实虚拟地址为vaddr。在做该操作时,请注意缓存行的边界*/
        dma_cache_sync(NULL, scbuf[tail].bf_vaddr, desc[tail].length, DMA_TO_DEVICE);
		desc[tail].BufPtr = (unsigned int) scbuf[tail].bf_paddr;
		desc[tail].OWN = 1;
		tail = next_tail(tail);
		offset += data_len;
	}

	dmabuf->tail = tail;

	if (need_start) {
		ar7240_i2sound_dma_desc((unsigned long) desc_p, mode);
		ar7240_i2sound_dma_start(mode);
	}
	else if (!sc->ppause) {
		ar7240_i2sound_dma_resume(mode);
	}

//    if (resume)
//        ar7240_i2sound_dma_resume(mode);

	I2S_UNLOCK(sc);

	return count - byte_cnt;
}

ssize_t ar9100_i2s_write(struct file * filp, const char __user * buf,
			 size_t count, loff_t * f_pos)
{
    int tmpcount, ret = 0;
    int cnt = 0;
    const char *data;

eagain:
    tmpcount = count;
    data = buf;
    ret = 0;

    do {
        ret = ar7240_i2s_write(filp, data, tmpcount, f_pos, 1);
        cnt++;
        if (ret == EAGAIN) {
            printk("%s:%d %d\n", __func__, __LINE__, ret);
            goto eagain;
        }

        tmpcount = tmpcount - ret;
        data += ret;
    } while(tmpcount > 0);

	printk("ar7240_i2s_write count %d cnt:%d\n", count, cnt);

    return count;
}


int ar7240_i2s_close(struct inode *inode, struct file *filp)
{
	int j, own, mode;
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	ar7240_mbox_dma_desc *desc;
    int status = TRUE;
    int own_count = 0;

    if (!filp) {
        mode  = 0;
    } else {
        mode = filp->f_mode;
    }

	if (mode & FMODE_READ) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
	}

	desc = dmabuf->db_desc;

	if (own) {
		for (j = 0; j < NUM_DESC; j++) {
			desc[j].OWN = 0;
		}
		ar7240_i2sound_dma_resume(mode);
    } else {
        for (j = 0; j < NUM_DESC; j++) {
            if (desc[j].OWN) {
                own_count++;
            }
        }

        /*
         * The schedule_timeout_interruptible is commented
         * as this function is called from other process
         * context, i.e. that of wlan device driver context
         * schedule_timeout_interruptible(HZ);
         */

        if (own_count > 0) {
            udelay((own_count * AOW_PER_DESC_INTERVAL) + DESC_FREE_WAIT_BUFFER);
            udelay(10000);

            for (j = 0; j < NUM_DESC; j++) {
                /* break if the descriptor is still not free*/
                if (desc[j].OWN) {
					printk("desc[%d].OWN:%d\n", j, desc[j].OWN);
                    status = FALSE;
                    printk("I2S : Fatal error\n");
                    break;
                }
            }
        }
    }

	for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf->db_buf[j].bf_paddr,
				 I2S_BUF_SIZE, DMA_BIDIRECTIONAL);
	}

	if (mode & FMODE_READ)
		kfree(sc->sc_rmall_buf);
	else
		kfree(sc->sc_pmall_buf);
	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

	if (mode & FMODE_READ) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}

	return (status);
}
#if 1
int ar7240_i2s_close_for_pcm(struct snd_pcm_substream *substream)
{
//	int j, own, mode;
	int j, own;
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;
	ar7240_mbox_dma_desc *desc;
    int status = TRUE;
    int own_count = 0;

//    if (!filp) {
//        mode  = 0;
//    } else {
//        mode = filp->f_mode;
//    }
//
//	if (mode & FMODE_READ) {
//		dmabuf = &sc->sc_rbuf;
//		own = sc->rpause;
//	} else {
//		dmabuf = &sc->sc_pbuf;
//		own = sc->ppause;
//	}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dmabuf = &sc->sc_rbuf;
		own = sc->rpause;
	} else {
		dmabuf = &sc->sc_pbuf;
		own = sc->ppause;
	}

	desc = dmabuf->db_desc;

	if (own) {
		for (j = 0; j < NUM_DESC; j++) {
			desc[j].OWN = 0;
		}
//		ar7240_i2sound_dma_resume(mode & FMODE_READ);
		ar7240_i2sound_dma_resume(substream->stream == SNDRV_PCM_STREAM_CAPTURE);
    } else {
        for (j = 0; j < NUM_DESC; j++) {
            if (desc[j].OWN) {
                own_count++;
            }
        }

        /*
         * The schedule_timeout_interruptible is commented
         * as this function is called from other process
         * context, i.e. that of wlan device driver context
         * schedule_timeout_interruptible(HZ);
         */

        if (own_count > 0) {
            udelay((own_count * AOW_PER_DESC_INTERVAL) + DESC_FREE_WAIT_BUFFER);
            udelay(10000);

            for (j = 0; j < NUM_DESC; j++) {
                /* break if the descriptor is still not free*/
                if (desc[j].OWN) {
					printk("desc[%d].OWN:%d\n", j, desc[j].OWN);
                    status = FALSE;
                    printk("I2S : Fatal error\n");
                    break;
                }
            }
        }
    }

	for (j = 0; j < NUM_DESC; j++) {
		dma_unmap_single(NULL, dmabuf->db_buf[j].bf_paddr,
				 I2S_BUF_SIZE, DMA_BIDIRECTIONAL);
	}

//	if (mode & FMODE_READ)
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		kfree(sc->sc_rmall_buf);
	else
		kfree(sc->sc_pmall_buf);
	dma_free_coherent(NULL,
			  NUM_DESC * sizeof(ar7240_mbox_dma_desc),
			  dmabuf->db_desc, dmabuf->db_desc_p);

//	if (mode & FMODE_READ) {
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		sc->ropened = 0;
		sc->rpause = 0;
	} else {
		sc->popened = 0;
		sc->ppause = 0;
	}

	return (status);
}
#endif

int ar7240_i2s_release(struct inode *inode, struct file *filp)
{
	printk(KERN_CRIT "release\n");
	return 0;
}

//int ar7240_i2s_ioctl(struct inode *inode, struct file *filp,
//		     unsigned int cmd, unsigned long arg)
long ar7240_i2s_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
#define AR7240_STEREO_CONFIG_DEFAULT (AR7240_STEREO_CONFIG_SPDIF_ENABLE | \
                AR7240_STEREO_CONFIG_ENABLE | \
                AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE | \
                AR7240_STEREO_CONFIG_MASTER | \
                AR7240_STEREO_CONFIG_PSEDGE(2))

//	int data, mask = 0, cab = 0, cab1 = 0, j, st_cfg = 0;
	int data, cab = 0, cab1 = 0, j, st_cfg = 0;
	struct ar7240_i2s_softc *sc = &sc_buf_var;
	i2s_dma_buf_t *dmabuf;

    if (filp->f_mode & FMODE_READ) {
        dmabuf = &sc->sc_rbuf;
    } else {
        dmabuf = &sc->sc_pbuf;
    }

	switch (cmd) {
    case I2S_PAUSE:
        data = arg;
	ar7240_i2sound_dma_pause(data);
	if (data) {
		sc->rpause = 1;
	} else {
		sc->ppause = 1;
	}
        return 0;
    case I2S_RESUME:
        data = arg;
	ar7240_i2sound_dma_resume(data);
	if (data) {
		sc->rpause = 0;
	} else {
		sc->ppause = 0;
	}
        return 0;
	case I2S_VOLUME:
		data = arg;
		if (data < 0)
		    data = 0;
        if (data > 23)
            data = 23;
		if (data < 16) {
            ar7240_reg_wr(AR7240_STEREO_VOLUME, ((1 << 5) + (16 - data)));
		} else {
            ar7240_reg_wr(AR7240_STEREO_VOLUME, (data - 16));
		}
		return 0;

	case I2S_FREQ:		/* Frequency settings */
		data = arg;
        switch (data) {
            case 44100:
                ar7240_i2s_clk(((0x11 << 16) + 0xb000), 0x2383);
                cab = SPDIF_CONFIG_SAMP_FREQ(SPDIF_SAMP_FREQ_44);
                cab1 = SPDIF_CONFIG_ORG_FREQ(SPDIF_ORG_FREQ_44);
                break;
            case 48000:
                ar7240_i2s_clk(((0x10 << 16) + 0x4fff), 0x181);
                cab = SPDIF_CONFIG_SAMP_FREQ(SPDIF_SAMP_FREQ_48);
                cab1 = SPDIF_CONFIG_ORG_FREQ(SPDIF_ORG_FREQ_48);
                break;
            default:
                //printk(KERN_CRIT "Freq %d not supported \n", data);
                return -ENOTSUPP;
        }
        for (j = 0; j < NUM_DESC; j++) {
            dmabuf->db_desc[j].Ca[0] |= cab;
            dmabuf->db_desc[j].Cb[0] |= cab;
            dmabuf->db_desc[j].Ca[1] |= cab1;
            dmabuf->db_desc[j].Cb[1] |= cab1;
        }
		return 0;

	case I2S_FINE:
		data = arg;
		return 0;

	case I2S_DSIZE:
		data = arg;
		switch (data) {
		case 8:
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT |
                 AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_8B));
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_8_16);
            break;
		case 16:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa21302);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                 AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B));
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_8_16);
            break;
		case 24:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa22b02);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_24B) |
                 AR7240_STEREO_CONFIG_I2S_32B_WORD);
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_24_32);
            break;
		case 32:
			//ar7240_reg_wr(AR7240_STEREO_CONFIG, 0xa22b02);
            st_cfg = (AR7240_STEREO_CONFIG_DEFAULT |
                AR7240_STEREO_CONFIG_PCM_SWAP |
                AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_32B) |
                 AR7240_STEREO_CONFIG_I2S_32B_WORD);
            cab1 = SPDIF_CONFIG_SAMP_SIZE(SPDIF_S_24_32);
            break;
		default:
			printk(KERN_CRIT "Data size %d not supported \n",
			       data);
			return -ENOTSUPP;
		}
        ar7240_reg_wr(AR7240_STEREO_CONFIG, (st_cfg | AR7240_STEREO_CONFIG_RESET));
        udelay(100);
        ar7240_reg_wr(AR7240_STEREO_CONFIG, st_cfg);
        for (j = 0; j < NUM_DESC; j++) {
            dmabuf->db_desc[j].Ca[1] |= cab1;
            dmabuf->db_desc[j].Cb[1] |= cab1;
        }
        return 0;

	case I2S_MODE:		/* mono or stereo */
		data = arg;
	    /* For MONO */
		if (data != 2) {
	        ar7240_reg_rmw_set(AR7240_STEREO_CONFIG, MONO);
        } else {
	        ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, MONO);
        }
		return 0;

    case I2S_MCLK:       /* Master clock is MCLK_IN or divided audio PLL */
		printk("Set MCLK IS NOT SUPPORT!!!\n");
		return 0;
		data = arg;
        if (data) {
            ar7240_reg_wr(AUDIO_PLL, AUDIO_PLL_RESET); /* Reset Audio PLL */
            ar7240_reg_rmw_set(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_I2S_MCLK_SEL);
        } else {
            ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_I2S_MCLK_SEL);
		}
		return 0;

	case I2S_COUNT:
		data = arg;
		return 0;

	default:
		return -ENOTSUPP;
	}


}

//irqreturn_t ar7240_i2s_intr(int irq, void *dev_id, struct pt_regs *regs)
irqreturn_t ar7240_i2s_intr(int irq, void *dev_id)
{
	uint32_t status;
	struct ath79_pcm_rt_priv *rtpriv;
	unsigned int period_bytes;

	status = ar7240_reg_rd(MBOX_INT_STATUS);

//    if(status & RX_UNDERFLOW)
//        stats.rx_underflow++;


	if(status & MBOX0_RX_DMA_COMPLETE) {
		unsigned int played_size;
		rtpriv = snd_chip.playback->runtime->private_data;
		/* Store the last played buffer in the runtime priv struct */
//		rtpriv->last_played = ath79_pcm_get_last_played(rtpriv);
		period_bytes = snd_pcm_lib_period_bytes(snd_chip.playback);

		played_size = ath79_pcm_set_own_bits(rtpriv);
        rtpriv->played_pos += played_size;
        if (rtpriv->played_pos >= snd_pcm_lib_buffer_bytes(snd_chip.playback))
        {
//            printk("played_pos !!!:%d\n", rtpriv->played_pos);
            rtpriv->played_pos -= snd_pcm_lib_buffer_bytes(snd_chip.playback);
        }

		if(played_size > period_bytes)
        {
//			snd_printd("Played more than one period  bytes played: %d\n",played_size);
        }
		rtpriv->elapsed_size += played_size;

//		ath79_mbox_interrupt_ack(AR934X_DMA_MBOX_INT_STATUS_RX_DMA_COMPLETE);
        /* Ack the interrupts */
        ar7240_reg_wr(MBOX_INT_STATUS, status);

        printk("elapsed:%u, played:%u, pos:%u\n",
            rtpriv->elapsed_size, 
            played_size, 
            rtpriv->played_pos);
		if(rtpriv->elapsed_size >= period_bytes)
		{
			rtpriv->elapsed_size %= period_bytes;
			snd_pcm_period_elapsed(snd_chip.playback);
		}

		if (rtpriv->last_played == NULL) {
//			snd_printd("BUG: ISR called but no played buf found\n");
			goto ack;
		}

	}
	if(status & AR934X_DMA_MBOX_INT_STATUS_TX_DMA_COMPLETE) {
		rtpriv = snd_chip.capture->runtime->private_data;
		/* Store the last played buffer in the runtime priv struct */
		rtpriv->last_played = ath79_pcm_get_last_played(rtpriv);
		ath79_pcm_set_own_bits(rtpriv);
//		ath79_mbox_interrupt_ack(AR934X_DMA_MBOX_INT_STATUS_TX_DMA_COMPLETE);
        /* Ack the interrupts */
        ar7240_reg_wr(MBOX_INT_STATUS, status);

		if (rtpriv->last_played == NULL) {
//			snd_printd("BUG: ISR called but no rec buf found\n");
			goto ack;
		}
		snd_pcm_period_elapsed(snd_chip.capture);
	}

ack:
	//printk("intrupt:%x\n", status);
	return IRQ_HANDLED;
}

void ar7240_i2sound_i2slink_on(int master)
{
	unsigned long rddata;
#if 0
    /* Clear all resets */
    ar7240_reg_wr(RST_RESET, 0);
    udelay(500);
#endif
    // Set GPIO I2S Enables
    // ---- I2S to JTAG (GPIO 6, 7, 8, 11, 12, JTAG_TCK)
    // Bit 31: SPDIF2TCK
    // Bit 30: SPDIF_EN
    // Bit 27: I2C_MCKEN
    // Bit 26: I2SO_EN
    // Bit  0: EJTAG_DISABLE
    // ---- I2S to GPIO 18~22 ----
    // Bit 30: SPDIF_EN -> Enables GPIO_23 or TCK as the SPDIF serial output
    // Bit 29: I2S_22_18_EN -> Enables GPIO bits [22:18] as I2S interface pins
    // Bit 27: I2C_MCKEN -> Enables the master audio CLK_MCK to be output through GPIO_21.
    // Bit 26: I2SO_EN -> Enables I2S
#ifdef I2S_GPIO_6_7_8_11_12
	/* GPIO_6_7_8和JTAG_TDI,JTAG_TDO,JTAG_TMS复用. GPIO_11_12和UART_RTS,UART_CTS复用 */
    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTIONS,
        (AR7240_GPIO_FUNCTION_I2S_MCKEN | AR7240_GPIO_FUNCTION_I2S0_EN | AR7240_GPIO_FUNCTION_JTAG_DISABLE));
#else
//AR7240_GPIO_FUNCTION_SPDIF_EN
	/* GPIO_18_22和SLIC复用 */
    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTIONS,
        (AR7240_GPIO_FUNCTION_I2S_GPIO_18_22_EN |
        AR7240_GPIO_FUNCTION_I2S_MCKEN | AR7240_GPIO_FUNCTION_I2S0_EN /* | AR7240_GPIO_FUNCTION_JTAG_DISABLE */));
#endif

	// ---- I2S to JTAG (GPIO 6, 7, 8, 11, 12, JTAG_TCK)
	// Bit 9: JUMP_START_DISABLE
    // ---- I2S to GPIO 18~22 ----
	// Bit 2: SPDIF_ON23 -> Enables the SPDIF output on GPIO23
#ifdef I2S_GPIO_6_7_8_11_12
	/* Disables Jumpstart input function on GPIO11 */
    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTION_2, (1<<9));
#else
	/* Enable the SPDIF output on GPIO23 */
    ar7240_reg_rmw_set(AR7240_GPIO_FUNCTION_2, (1<<2));
#endif
	// Set GPIO_OE
    ar7240_reg_rmw_set(AR7240_GPIO_OE, (1<<I2S_GPIOPIN_SCK) | (1<<I2S_GPIOPIN_WS) | (1<<I2S_GPIOPIN_SD) | (1<<I2S_GPIOPIN_OMCLK));
    ar7240_reg_rmw_clear(AR7240_GPIO_OE, (1<<I2S_GPIOPIN_MIC));

	//Set PLL
	/* CPU clock is 400MHz :
	 * 400 / 11.2896(MHz) = 35.430839002267573696145124716553
	 * 35.430839002267573696145124716553 / 2 = 17.715419501133786848072562358277
	 * INT = 0x11, FRAC = 0xb725 (0xffff * 0.715419501133786848072562358277)
	 */
	/* CPU clock is 300MHz :
	 * 300 / 11.2896(MHz) = 26.573129251700680272108843537415
	 * 26.573129251700680272108843537415 / 2 = 13.286564625850340136054421768707
	 * INT = 0x0D, FRAC = 0x4964 (0xffff * 0.286564625850340136054421768707)
	 */

	//rddata = (0x11 << 16) + 0xb724;
	//rddata = (0x0d << 16) + 0x12bc;

//	rddata = (0x10 << 16) + 0x4fff; //  for 48000 x 2 bytes x 2 channels = 192000
//	rddata = (0x11 << 16) + 0xc8ff; //  for 44100 x 2 bytes x 2 channels = 176400
	rddata = (0x11 << 16) + 0xb725; //  for 44100 x 2 bytes x 2 channels = 176400
	ar7240_reg_wr(AR7240_STEREO_CLK_DIV, rddata);

    /*
     * AR7240_STEREO_CONFIG should carry 0x201302 for MIC and I2S
     * AR7240_STEREO_CONFIG should carry 0xa01302 for SPDIF
     */
     //
     ar7240_reg_wr(AR7240_STEREO_CONFIG,
       (AR7240_STEREO_CONFIG_RESET |
        AR7240_STEREO_CONFIG_SPDIF_ENABLE |
        AR7240_STEREO_CONFIG_PCM_SWAP |
        AR7240_STEREO_CONFIG_ENABLE |
//		AR7240_STEREO_CONFIG_DELAY |
        AR7240_STEREO_CONFIG_DATA_WORD_SIZE(AR7240_STEREO_WS_16B) |
//		AR7240_STEREO_CONFIG_I2S_32B_WORD |
        AR7240_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
        AR7240_STEREO_CONFIG_MASTER |
        AR7240_STEREO_CONFIG_PSEDGE(2)));

    udelay(100);
    ar7240_reg_rmw_clear(AR7240_STEREO_CONFIG, AR7240_STEREO_CONFIG_RESET);
}

void ar7240_i2sound_request_dma_channel(int mode)
{
    //ar7240_reg_wr(MBOX_DMA_POLICY, 0x6a);
    ar7240_reg_wr(MBOX_DMA_POLICY, 0x4a);
}

void ar7240_i2sound_dma_desc(unsigned long desc_buf_p, int mode)
{
	/*
	 * Program the device to generate interrupts
	 * RX_DMA_COMPLETE for mbox 0
	 */
	//sizeof(unsigned long) == 4Byte
	printk("desc_buf_p:%lu\n", desc_buf_p);
	//long desc_buf_p = (desc_buf_p >> 4) & 0x0ffffffc;
	printk("desc_buf_p:%lu\n", desc_buf_p);
	if (mode) {
		ar7240_reg_wr(MBOX0_DMA_TX_DESCRIPTOR_BASE, desc_buf_p);
	} else {
		ar7240_reg_wr(MBOX0_DMA_RX_DESCRIPTOR_BASE, desc_buf_p);
	}
}

void ar7240_i2sound_dma_start(int mode)
{
	/*
	 * Start
	 */
	if (mode) {
		ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, START);
	} else {
		ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, START);
	}
}
EXPORT_SYMBOL(ar7240_i2sound_dma_start);

void ar7240_i2sound_dma_pause(int mode)
{
    /*
        * Pause
        */
    if (mode) {
        ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, PAUSE);
    } else {
        ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, PAUSE);
    }

}
EXPORT_SYMBOL(ar7240_i2sound_dma_pause);

void ar7240_i2sound_dma_resume(int mode)
{
    /*
        * Resume
        */
    if (mode) {
        ar7240_reg_wr(MBOX0_DMA_TX_CONTROL, RESUME);
    } else {
        ar7240_reg_wr(MBOX0_DMA_RX_CONTROL, RESUME);
    }
}
EXPORT_SYMBOL(ar7240_i2sound_dma_resume);

void ar7240_i2s_clk(unsigned long frac, unsigned long pll)
{
/* Note: the interpretation of frac is different between Honet and Wasp */
#if 1 /* Hornet */
	ar7240_reg_wr(AR7240_STEREO_CLK_DIV, frac);
#else
#define ATH_AUDIO_PLL_CONFIG_PWR_DWN (1ul << 5)
    /*
     * Tick...Tick...Tick
     */
    ar7240_reg_wr(FRAC_FREQ, frac);
    ar7240_reg_wr(AUDIO_PLL, pll);
	ar7240_reg_wr(AR7240_AUDIO_PLL_CONFIG, (pll & ~ATH_AUDIO_PLL_CONFIG_PWR_DWN));
#endif
}
EXPORT_SYMBOL(ar7240_i2s_clk);


//struct snd_pcm_hardware {
//	unsigned int info;		/* SNDRV_PCM_INFO_* */
//	u64 formats;			/* SNDRV_PCM_FMTBIT_* */
//	unsigned int rates;		/* SNDRV_PCM_RATE_* */
//	unsigned int rate_min;		/* min rate */
//	unsigned int rate_max;		/* max rate */
//	unsigned int channels_min;	/* min channels */
//	unsigned int channels_max;	/* max channels */
//	size_t buffer_bytes_max;	/* max buffer size */
//	size_t period_bytes_min;	/* min period size */
//	size_t period_bytes_max;	/* max period size */
//	unsigned int periods_min;	/* min # of periods */
//	unsigned int periods_max;	/* max # of periods */
//	size_t fifo_size;		/* fifo size in bytes */
//};
// 1 frame = channels * sample_size
// chunk_bytes = peroid_size * sample_length / 8。chunk_bytes就是我们单次从WAV读PCM数据的大小。

#define BUFFER_BYTES_MAX 16 * 4095 * 16
#define PERIOD_BYTES_MIN 64

struct snd_pcm_hardware ar7240_i2s_alsa_pcm_hardware =
{
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
//		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		SNDRV_PCM_INFO_MMAP_VALID,
	//S8:     signed   8 bits,有符号字符 = char,          表示范围 -128~127
	//U8:     unsigned 8 bits,无符号字符 = unsigned char,表示范围 0~255
	//S16_LE: little endian signed 16 bits,小端有符号字 = short,表示范围 -32768~32767
	//S16_BE: big endian signed 16 bits,大端有符号字 = short倒序(PPC),表示范围 -32768~32767
	//U16_LE: little endian unsigned 16 bits,小端无符号字 = unsigned short,表示范围 0~65535
	//U16_BE: big endian unsigned signed 16 bits,大端无符号字 = unsigned short倒序(PPC),表示范围 0~65535
	.formats = SNDRV_PCM_FMTBIT_S8 |
		SNDRV_PCM_FMTBIT_S16_BE |
		SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S24_BE |
		SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_S32_BE |
		SNDRV_PCM_FMTBIT_S32_LE,

	.rates =        SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
	.rate_min =     44100,
	.rate_max =     48000,
	.channels_min = 2,
	.channels_max = 2,
	/* Analog audio output will be full of clicks and pops if there
	 are not exactly four lines in the SRAM FIFO buffer.  */
//	.buffer_bytes_max = BUFFER_BYTES_MAX,
//	.period_bytes_min = PERIOD_BYTES_MIN,
//	.period_bytes_max = 4095,
//	.periods_min = 16,
//	.periods_max = 256,
	.buffer_bytes_max = 16*4095*16,
	.period_bytes_min = 64,
	.period_bytes_max = 4095,
	.periods_min = 16,
	.periods_max = 256,
	.fifo_size = 0,
};

static int ar7240_i2s_alsa_pcm_open(struct snd_pcm_substream *substream)
{
	int ret;
//    int mode = 0; //playback
//    int need_start = 0;
	struct ath79_pcm_rt_priv *rtpriv;
	struct snd_pcm_runtime *runtime = substream->runtime;
//	struct snd_dma_buffer *buf = &substream->dma_buffer;
//	ar7240_i2s_softc_t *sc = &sc_buf_var;
//	unsigned long desc_p = (unsigned long) sc->sc_pbuf.db_desc_p;

    snd_chip.playback = substream;

	runtime->hw = ar7240_i2s_alsa_pcm_hardware;

	/* ensure buffer_size is a multiple of period_size */  
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
	{
		printk(KERN_CRIT "buffer_size is not a multiple of period_size\n");
		return ret;
	}

	/* Allocate/Initialize the buffer linked list head */
	rtpriv = kmalloc(sizeof(*rtpriv), GFP_KERNEL);
	if (!rtpriv) {
		return -ENOMEM;
	}
	snd_printd("%s: 0x%xB allocated at 0x%08x\n",
	       __FUNCTION__, sizeof(*rtpriv), (u32) rtpriv);

	substream->runtime->private_data = rtpriv;
	rtpriv->last_played = NULL;
    rtpriv->played_pos = 0;
	INIT_LIST_HEAD(&rtpriv->dma_head);
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		rtpriv->direction = SNDRV_PCM_STREAM_PLAYBACK;
	else
		rtpriv->direction = SNDRV_PCM_STREAM_CAPTURE;

//	ret = ar7240_i2s_open_for_pcm(substream);
//	if (ret < 0)
//	{
//		printk(KERN_CRIT "open fail:%d\n", ret);
//		return ret;
//	}
//	if (sc->popened < 2) {
//        ar7240_reg_rmw_set(MBOX_INT_ENABLE, MBOX0_RX_DMA_COMPLETE | RX_UNDERFLOW);
//		need_start = 1;
//	}
//    sc->popened = 2;
//
//	if (need_start) {
//		ar7240_i2sound_dma_desc((unsigned long) desc_p, mode);
//		ar7240_i2sound_dma_start(mode);
//	}
//
//	buf->dev.type = SNDRV_DMA_TYPE_DEV;
//	buf->dev.dev = substream->pcm->card->dev;
//	buf->private_data = NULL;
//	buf->bytes = ar7240_i2s_alsa_pcm_hardware.buffer_bytes_max;
//
//    buf->area = (unsigned char *)(sc->sc_pmall_buf);
//    buf->addr = sc->sc_pbuf.db_desc_p;

//	buf->area = dma_alloc_coherent(NULL, buf->bytes,
//					   &buf->addr, GFP_DMA);
//	if (!buf->area)
//		ret = -ENOMEM;
	//snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	printk(KERN_CRIT "pcm open:%d\n", ret);
	return 0;
}
static int ar7240_i2s_alsa_pcm_close(struct snd_pcm_substream *substream)
{
	struct ath79_pcm_rt_priv *rtpriv;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_chip.playback = NULL;
	} else {
		snd_chip.capture = NULL;
	}

	rtpriv = substream->runtime->private_data;
	kfree(rtpriv);

//	ret = ar7240_i2s_close_for_pcm(substream);
	printk(KERN_CRIT "pcm close:%d\n", 0);
	return 0;
}

static void print_hwparams(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *p)
{
	char name[16];
	snd_pcm_debug_name(substream, name, sizeof(name));
	printk("%s HWPARAMS\n", name);
	printk(" samplerate %d Hz\n", params_rate(p));
	printk(" channels %d\n", params_channels(p));
	printk(" format %d\n", params_format(p));
	printk(" subformat %d\n", params_subformat(p));
	printk(" buffer %d B\n", params_buffer_bytes(p));
	printk(" period %d B\n", params_period_bytes(p));
	printk(" access %d\n", params_access(p));
	printk(" period_size %d\n", params_period_size(p));
	printk(" periods %d\n", params_periods(p));
	printk(" buffer_size %d\n", params_buffer_size(p));
	printk(" %d B/s\n", params_rate(p) *
		params_channels(p) *
		snd_pcm_format_width(params_format(p)) / 8);

}
static int 
ar7240_i2s_alsa_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
//	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
//	runtime->dma_bytes = params_buffer_bytes(params);
//	ar7240_i2s_softc_t *sc = &sc_buf_var;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ath79_pcm_rt_priv *rtpriv;
	int ret;
	unsigned int period_size, sample_size, sample_rate, frames, channels;

	// Does this routine need to handle new clock changes in the hw_params?
	rtpriv = runtime->private_data;

	ret = ath79_mbox_dma_map(rtpriv, substream->dma_buffer.addr,
		params_period_bytes(hw_params), params_buffer_bytes(hw_params));

	if(ret < 0)
		return ret;

	period_size = params_period_bytes(hw_params);
	sample_size = snd_pcm_format_size(params_format(hw_params), 1);
	sample_rate = params_rate(hw_params);
	channels = params_channels(hw_params);
	frames = period_size / (sample_size * channels);

/* 	When we disbale the DMA engine, it could be just at the start of a descriptor.
	Hence calculate the longest time the DMA engine could be grabbing bytes for to
	Make sure we do not unmap the memory before the DMA is complete.
	Add 10 mSec of margin. This value will be used in ath79_mbox_dma_stop */

	rtpriv->delay_time = (frames * 1000)/sample_rate + 10;


	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(hw_params);

    //snd_pcm_lib_malloc_pages()仅仅当DMA缓冲区已经被预分配之后(snd_pcm_lib_preallocate_pages_for_all)才可以用。
    //采用标准的内存分配函数snd_pcm_lib_malloc_pages(),就不再需要自己设定DMA缓冲区信息
//    ret = snd_pcm_lib_malloc_pages(substream,
//					params_buffer_bytes(params));
    print_hwparams(substream, hw_params);
//    runtime->dma_area = (unsigned char *)(sc->sc_pmall_buf);
//    runtime->dma_addr = sc->sc_pbuf.db_desc_p;
//    snd_pcm_write
//	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	printk(KERN_CRIT "pcm hw_params:%d\n", ret);
	return 1;
}

static int ar7240_i2s_alsa_pcm_hw_free(struct snd_pcm_substream *substream)
{
    int ret = 0;
//	ret = snd_pcm_lib_free_pages(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ath79_pcm_rt_priv *rtpriv;

	rtpriv = runtime->private_data;
	ath79_mbox_dma_unmap(rtpriv);
	snd_pcm_set_runtime_buffer(substream, NULL);
	printk(KERN_CRIT "pcm hw_free:%d\n", ret);
    return ret;
}
static int ar7240_i2s_alsa_pcm_prepare(struct snd_pcm_substream *substream)
{
    int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ath79_pcm_rt_priv *rtpriv;

	rtpriv = runtime->private_data;
//	cpu_dai = rtd->cpu_dai;
//	/* When setup the first stream should reset the DMA MBOX controller */
//	if(cpu_dai->active == 1)
//		ath79_mbox_dma_reset();

	ath79_mbox_dma_prepare(rtpriv);

	ath79_pcm_set_own_bits(rtpriv); //设置为1,表示有数据可以播放
	rtpriv->last_played = NULL;
	printk(KERN_CRIT "pcm prepare:%d\n", ret);
	return ret;
}
static int ar7240_i2s_alsa_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    int data = 0;
	struct ath79_pcm_rt_priv *rtpriv = substream->runtime->private_data;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
        ar7240_i2sound_dma_start(data);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
        ar7240_i2sound_dma_pause(data);

        /* Delay for the dynamically calculated max time based on
        sample size, channel, sample rate + margin to ensure that the
        DMA engine will be truly idle. */
        mdelay(rtpriv->delay_time);

		break;
	case SNDRV_PCM_TRIGGER_RESUME:
        ar7240_i2sound_dma_resume(data);
		break;
	default:
		return -EINVAL;
	}
	printk(KERN_CRIT "pcm trigger :%d\n", cmd);
	return 0;
}
static snd_pcm_uframes_t ar7240_i2s_alsa_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ath79_pcm_rt_priv *rtpriv;
	snd_pcm_uframes_t ret = 0;

//如果没有中断,pointer函数只被调用一次,就是在 trigger 1 之后,事实证明中断必不可少
//        unsigned int period_bytes;
//		unsigned int played_size;
//		rtpriv = substream->runtime->private_data;
//		/* Store the last played buffer in the runtime priv struct */
//		period_bytes = snd_pcm_lib_period_bytes(snd_chip.playback);
//
//		played_size = ath79_pcm_set_own_bits(rtpriv);
//        rtpriv->played_pos += played_size;
//            printk("played_pos !!!:%d\n", rtpriv->played_pos);
//        if (rtpriv->played_pos >= snd_pcm_lib_buffer_bytes(snd_chip.playback))
//        {
//            rtpriv->played_pos -= snd_pcm_lib_buffer_bytes(snd_chip.playback);
//        }

	rtpriv = runtime->private_data;

//	if(rtpriv->last_played == NULL)
//		ret = 0;
//	else
//		ret = rtpriv->last_played->BufPtr - runtime->dma_addr;

    ret = rtpriv->played_pos;
	ret = bytes_to_frames(runtime, ret);
	printk(KERN_CRIT "pcm pointer:%lu\n", ret);
	return ret;
}
//static struct page *ar7240_i2s_alsa_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
//{
//	printk(KERN_CRIT "pcm page\n");
//	return NULL;
//}
static int ar7240_i2s_alsa_pcm_mmap(struct snd_pcm_substream *ss, struct vm_area_struct *vma)
{
	printk(KERN_CRIT "pcm mmap\n");
	return remap_pfn_range(vma, vma->vm_start,
			ss->dma_buffer.addr >> PAGE_SHIFT,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static int ar7240_i2s_alsa_pcm_ack(struct snd_pcm_substream *ss)
{
	printk(KERN_CRIT "pcm ack\n");
    return 0;
}

static struct snd_pcm_ops snd_933x_pcm_playback = {
	.open = ar7240_i2s_alsa_pcm_open, //为PCM设定支持的传输模式,数据格式,通道数,period等,分配相应的DMA通道
	.close = ar7240_i2s_alsa_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = ar7240_i2s_alsa_pcm_hw_params, //当pcm子流中已经定义了缓冲区大小，period大小，格式等的时候，应用程序可以通过这个函数来设定硬件参数。
	.hw_free = ar7240_i2s_alsa_pcm_hw_free, //hw_params的相反操作
	.prepare = ar7240_i2s_alsa_pcm_prepare,
    //当pcm“准备好了”的时候调用这个函数。和hw_params不同的是，每次调用snd_pcm_prepare()的时候都要调用prepare函数。
	.trigger = ar7240_i2s_alsa_pcm_trigger, //当pcm开始、停止、暂停的时候都会调用trigger函数
	.pointer = ar7240_i2s_alsa_pcm_pointer, //PCM中间层通过调用这个函数来获取缓冲区的位置,alsa调用这个函数轮询dma的进度。
    //.wall_clock = ar7240_i2s_alsa_pcm_wall_clock,
    //.copy = ,
    //.silence = ar7240_i2s_alsa_pcm_silence, //同copy,aio_write调用
	//.page    = ar7240_i2s_alsa_pcm_page, //这个函数也不是必须的。这个函数主要对那些不连续的缓存区。mmap会调用这个函数得到内存页的地址。
	.mmap    = ar7240_i2s_alsa_pcm_mmap,
    .ack     = ar7240_i2s_alsa_pcm_ack, //这个函数也不是必须的。当在读写操作的时候更新appl_ptr的时候会调用它。
};

static void ath79_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *ss;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		ss = pcm->streams[stream].substream;
		if (!ss)
			continue;
		buf = &ss->dma_buffer;
		if (!buf->area)
			continue;
		dma_free_coherent(NULL, buf->bytes,
				      buf->area, buf->addr);
        printk("dma_free_coherent\n");
		buf->area = NULL;
	}

	ath79_mbox_dma_exit();
}

static int ath79_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *ss = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &ss->dma_buffer;

	printk(KERN_NOTICE "%s: allocate %8s stream\n", __FUNCTION__,
		stream == SNDRV_PCM_STREAM_CAPTURE ? "capture" : "playback" );

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = ar7240_i2s_alsa_pcm_hardware.buffer_bytes_max;

	buf->area = dma_alloc_coherent(NULL, buf->bytes,
					   &buf->addr, GFP_DMA);
	if (!buf->area)
		return -ENOMEM;

	printk(KERN_NOTICE "%s: 0x%xB allocated at 0x%08x\n",
		__FUNCTION__, buf->bytes, (u32) buf->area);

	return 0;
}

static int ar7240_i2s_alsa_cleanup(void)
{
	if (snd_chip.card)
	{
        ath79_pcm_free_dma_buffers(snd_chip.pcm);
		snd_card_free(snd_chip.card);
        snd_chip.card = NULL;
        snd_chip.pcm = NULL;
        snd_chip.playback = NULL;
        snd_chip.capture = NULL;
	}
	return 0;
}

static int ar7240_i2s_alsa_init(void)
{
	int result = -1;
	struct snd_card *card;
	struct snd_pcm *pcm;
	//如果传入的声卡编号为-1,自动分配一个索引编号
	result = snd_card_create(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1, THIS_MODULE, 0, &card);
	if(result < 0)
	{
		goto error;
	}
    snd_chip.card = card;

	strcpy(card->driver, "I2S ALSA CARD");
	sprintf(card->shortname, "i2s alsa card");
	sprintf(card->longname, "ar9331 i2s alsa card");

	result = snd_pcm_new(card, "i2s alsa pcm", 0, 1, 0, &pcm);
	if(result < 0)
	{
		goto error;
	}
    snd_chip.pcm = pcm;
	result = ath79_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
	ath79_mbox_dma_init(NULL);

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_933x_pcm_playback);
	pcm->info_flags = 0;
	strcpy(pcm->name, "i2s alsa pcm playback");

//    result = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV, NULL, 2*768, 2*768);
//	if(result < 0)
//	{
//        printk(KERN_CRIT "snd_pcm_lib_preallocate_pages_for_all:%d", result);
//		goto error;
//	}
	//control设备则在snd_card_create()内被创建,无需显式地创建control设备,只要建立声卡,control设备被自动地创建
	//result = snd_ctl_create(card);
	//result = snd_info_card_create(card); 

	result = snd_card_register(card);
	if(result < 0)
	{
		goto error;
	}

	return 0;		/* succeed */

error:
	if (card)
	{
		snd_card_free(card);
        snd_chip.card = NULL;
	}
	printk(KERN_CRIT "sound alsa fila\n");
	return -1;
}

struct file_operations ar7240_i2s_fops = {
	.owner = THIS_MODULE,
	.read = ar7240_i2s_read,
	.write = ar9100_i2s_write,
	.unlocked_ioctl = ar7240_i2s_ioctl,
	.open = ar7240_i2s_open,
	.release = ar7240_i2s_close,
};

void ar7240_i2s_cleanup_module(void)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;

	printk(KERN_CRIT "unregister\n");

	free_irq(sc->sc_irq, NULL);
	unregister_chrdev(ar7240_i2s_major, "ath_i2s");
	device_destroy(sc->i2sclass, MKDEV(ar7240_i2s_major, ar7240_i2s_minor));
	class_destroy(sc->i2sclass);
    i2c_del_driver(&wm8918_driver);
	ar7240_i2s_alsa_cleanup();
}

int ar7240_i2s_init_module(void)
{
	ar7240_i2s_softc_t *sc = &sc_buf_var;
	int result = -1;
	int master = 1;


	memset(sc, 0,sizeof(ar7240_i2s_softc_t));

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (ar7240_i2s_major) {
		result =
		    register_chrdev(ar7240_i2s_major, "ath_i2s",
				    &ar7240_i2s_fops);
	}
	if (result < 0) {
		printk(KERN_WARNING "ar7240_i2s: can't get major %d\n",
		       ar7240_i2s_major);
		return result;
	}

	sc->i2sclass = class_create(THIS_MODULE, "ar9331_i2s");
	if(IS_ERR(sc->i2sclass)) 
	{
		printk("Err: failed in creating class./n");
		return -1; 
	}
	device_create(sc->i2sclass, NULL, MKDEV(ar7240_i2s_major, ar7240_i2s_minor), NULL, "i2s");

//	sc->sc_irq = AR7240_MISC_IRQ_DMA;
	sc->sc_irq = ATH79_MISC_IRQ(7);

	/* Establish ISR would take care of enabling the interrupt */
	result = request_irq(sc->sc_irq, ar7240_i2s_intr, AR7240_IRQF_DISABLED,
			     "ar7240_i2s", NULL);
	if (result) {
		printk(KERN_INFO
		       "i2s: can't get assigned irq %d returns %d\n",
		       sc->sc_irq, result);
	}

	ar7240_i2sound_i2slink_on(master);

	I2S_LOCK_INIT(sc);

	//wm8918控制i2c接口
    i2c_add_driver(&wm8918_driver);
	/* 0x180b0000是物理地址,他在虚拟内存中的地址为0xb80b0000,下面两个打印相同 */
//	printk(KERN_CRIT "%08x, %08x\n", KSEG1ADDR(0x180b0000), KSEG1ADDR(0xb80b0000));

	ar7240_i2s_alsa_init();

	return 0;
}

module_init(ar7240_i2s_init_module);
module_exit(ar7240_i2s_cleanup_module);
