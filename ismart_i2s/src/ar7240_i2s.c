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
//#include <linux/config.h>


#include "i2sio.h"
#include "ar7240.h"
#include "ar7240_i2s.h"		/* local definitions */

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
} ar7240_i2s_softc_t;


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

		/*对由dma_alloc_noncoherent()申请的内存做局部映射，其实虚拟地址为vaddr。在做该操作时，请注意缓存行的边界*/
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
	unsigned long startTime ;

eagain:
    tmpcount = count;
    data = buf;
    ret = 0;

	startTime = jiffies;

    do {
        ret = ar7240_i2s_write(filp, data, tmpcount, f_pos, 1);
        cnt++;
        if (ret == EAGAIN) {
            printk("%s:%d %d\n", __func__, __LINE__, ret);
            goto eagain;
        }

        tmpcount = tmpcount - ret;
        data += ret;
//		printk("ar7240_i2s_write ret:%d\n", ret);
    } while(tmpcount > 0);

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
	uint32_t r;
//        static u_int16_t tglFlg = 0;

	r = ar7240_reg_rd(MBOX_INT_STATUS);
	#if 1 /* Hornet*/
		/* No LED on Hornet */
	#else /* Wasp*/
        if (r & MBOX0_RX_DMA_COMPLETE) {
            if (tglFlg) {
                ar7240_reg_wr(AR7240_GPIO_SET, 1<< 6);
            } else {
                ar7240_reg_wr(AR7240_GPIO_CLEAR, 1<< 6);
            }
            tglFlg = !tglFlg;
        }
	#endif

    if(r & RX_UNDERFLOW)
        stats.rx_underflow++;

	/* Ack the interrupts */
	ar7240_reg_wr(MBOX_INT_STATUS, r);

	printk("intrupt\n");
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
//	rddata = (0x11 << 16) + 0xb000; //  for 44100 x 2 bytes x 2 channels = 176400
	rddata = (0x08 << 16) + 0x0000; //  for 96000 x 2 bytes x 2 channels = 384000
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
	printk("desc_buf_p:%u\n", desc_buf_p);
	//long desc_buf_p = (desc_buf_p >> 4) & 0x0ffffffc;
	printk("desc_buf_p:%u\n", desc_buf_p);
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

	sc->sc_irq = AR7240_MISC_IRQ_DMA;

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

	/* 0x180b0000是物理地址,他在虚拟内存中的地址为0xb80b0000,下面两个打印相同 */
	printk(KERN_CRIT "addr: %u, %u\n", KSEG1ADDR(0x180b0000), KSEG1ADDR(0xb80b0000));

	return 0;		/* succeed */
}

module_init(ar7240_i2s_init_module);
module_exit(ar7240_i2s_cleanup_module);
