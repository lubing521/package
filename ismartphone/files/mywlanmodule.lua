
require("luci.json")
require("luci.model.uci")
require("luci.model.network")
require("luci.sys")
module("luci.controller.myapp.mywlanmodule", package.seeall)

function index()
	entry({"net"}, firstchild(), _("net"), 89)
	entry({"net", "wlan"}, firstchild(), _("netwan"), 89)
	entry({"net", "wlan", "wlanstatus"}, call("myfun_wlanstatus"), _("wlanstatus"), 89)
	entry({"net", "wlan", "disable"}, call("myfun_disable"), _("disable"), 89)
	entry({"net", "wlan", "enable"}, call("myfun_enable"), _("enable"), 89)
	entry({"net", "wlan", "setssid"}, call("myfun_setssid"), _("setssid"), 89)
	entry({"net", "wlan", "setwifipasswd"}, call("myfun_setwifipasswd"), _("setwifipasswd"), 89)
	entry({"net", "wlan", "wifipasswdsecurity"}, call("myfun_wifipasswdsecurity"), _("wifipasswdsecurity"), 89)
	entry({"net", "wlan", "scanwifi"}, call("myfun_scanwifi"), _("scanwifi"), 89)
	entry({"net", "wlan", "assoclist"}, call("myfun_assoclist"), _("assoclist"), 89)
	entry({"net", "wlan", "channelquality"}, call("myfun_channelquality"), _("channelquality"), 89)
	entry({"net", "wlan", "betterchannelquality"}, call("myfun_betterchannelquality"), _("betterchannelquality"), 89)
	entry({"net", "wlan", "signalstrength"}, call("myfun_signalstrength"), _("signalstrength"), 89)
	entry({"net", "wlan", "bettersignalstrength"}, call("myfun_bettersignalstrength"), _("bettersignalstrength"), 89)
	entry({"net", "wlan", "setsignalstrength"}, call("myfun_setsignalstrength"), _("setsignalstrength"), 89)
	entry({"net", "wlan", "timing"}, call("myfun_timing"), _("timing"), 89)
	entry({"net", "wlan", "cleartiming"}, call("myfun_cleartiming"), _("cleartiming"), 89)
	entry({"net", "wlan", "gettiming"}, call("myfun_gettiming"), _("gettiming"), 89)
	entry({"net", "wlan", "unlink"}, call("myfun_unlink"), _("unlink"), 89)
	entry({"net", "wlan", "unlinklist"}, call("myfun_unlinklist"), _("unlinklist"), 89)
	entry({"net", "wlan", "link"}, call("myfun_link"), _("link"), 89)
	entry({"net", "wlan", "alllink"}, call("myfun_alllink"), _("alllink"), 89)
	entry({"net", "wlan", "wlanjoin"}, call("myfun_wlanjoin"), _("wlanjoin"), 89)
	entry({"net", "wlan", "get_wandev"}, call("myfun_get_wandev"), _(get_wandev), 89)
	entry({"net", "wlan", "get_wannet"}, call("myfun_get_wannet"), _(get_wannet), 89)
end

function myfun_wlanstatus()
	local uci = luci.model.uci.cursor()
        local ntm = require "luci.model.network".init()
	local disabled
	local ssid
	local channel
	local r = true
	disabled = uci:get("wireless", "radio0", "disabled")
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		channel = net:channel()
		break
	end
	for _, net in ipairs(dev:get_wifinets()) do
		if net:mode() == "ap" then
			ssid = net:ssid()
			break
		end
	end
	local v = {disabled=disabled,ssid=ssid,channel=channel} 
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_disable()
	local uci = luci.model.uci.cursor()
	local r = uci:set("wireless", "radio0", "disabled","1")
	local r = uci:commit("wireless")
	uci:apply("wireless")
	local s = {result=r}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_enable()
	local uci = luci.model.uci.cursor()
	local r = uci:set("wireless", "radio0", "disabled","0")
	local r = uci:commit("wireless")
	uci:apply("wireless")
	local s = {result=r}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_setssid()
	local ssid = luci.http.formvalue("ssid")
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	local r
	for _, net in ipairs(dev:get_wifinets()) do
		if net:mode() == "ap" then
			r = net:set("ssid", ssid)
			ntm:commit("wireless")
			luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
			break
		end
	end
	local s = {result=r}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_setwifipasswd()
	local r = false
	local passwd = luci.http.formvalue("passwd")
	local oldpasswd = luci.http.formvalue("oldpasswd")
	local ucipasswd
	local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	if passwd == nil or passwd == "" or oldpasswd == "" or oldpasswd == nil or #passwd < 8 then
		r = false
	else
		for _, net in ipairs(dev:get_wifinets()) do
			if net:mode() == "ap" then
				ucipasswd = net:get("key")
				if ucipasswd == oldpasswd then
					r = true
					net:set("key", passwd)
					net:set("encryption", "psk-mixed")
					ntm:commit("wireless")
					--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
					luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
				end
				break
			end
		end
	end
	local s = {result=r, passwd=#passwd}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end


function myfun_wifipasswdsecurity()
	local r = true
	local security = 0
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		if net:mode() == "ap" then
			if net:active_encryption() == "none" then
				security = 1
			elseif net:get("key") == "11111111" then
				security = 2
			elseif tonumber(net:get("key")) ~= nil then
				security = 3
			end
			break
		end
	end
	
	local v = {security = security} 
	local s = {result=r,value = v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_scanwifi()
	local iw = luci.sys.wifi.getiwinfo("radio0")
	local rv = {}
	luci.http.prepare_content("application/json")                           
	for k, v in ipairs(iw.scanlist) do
		local rd = 
		{
			ssid=v.ssid,
			bssid=v.bssid,
			signal=v.signal,
			channel=v.channel,
			mode=v.mode,
			encrydescription=v.encryption.description,
			encrywpa=v.encryption.wpa,
		}
		rv[#rv+1] = rd
	end
	local r = true
	local v = rv 
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_assoclist()
	local rv = {}
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		local rd = 
		{
			ssid=net:ssid(),
			assoclist=net:assoclist()
		}
		rv[#rv+1] = rd
		--luci.http.write_json(net:assoclist())
	end
	local r = true
	local v = rv 
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_channelquality()
	local r = true
	local v = {quality = 0}
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_betterchannelquality()
	local r = true
	local v = {quality = 0}
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_signalstrength()
	local ntm = require "luci.model.network".init()
	local r = true
	local dev = ntm:get_wifidev("radio0")
	local txpower = dev:get("txpower")
	local numtxpower = tonumber(txpower)
	if numtxpower > 20 then
		txpower = "0"
	elseif numtxpower > 13 and numtxpower <= 20 then
		txpower = "1"
	else
		txpower = "2"
	end
	--for _, net in ipairs(dev:get_wifinets()) do
	--	txpower = net:txpower()
	--	break
	--end
	local v = {strength=txpower} 
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_bettersignalstrength()
	local ntm = require "luci.model.network".init()
	local r = true
	local v
	local strength = luci.http.formvalue("strength")              
	local power = strength
	if power == "0" then
		power = "27"
	elseif power == "1" then
		power = "18"
	elseif power == "2" then
		power = "10"
	else
		r = false
	end
	if r then
		local dev = ntm:get_wifidev("radio0")
		dev:set("txpower", power)
		ntm:commit("wireless")
		--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
		luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
		v = {strength=strength} 
	end
	local s = {result=r,value = v}
	luci.http.write_json(s)

	
end

function myfun_setsignalstrength()
	local ntm = require "luci.model.network".init()
	local power = luci.http.formvalue("strength")              
	local r = true
	local dev = ntm:get_wifidev("radio0")
	dev:set("txpower", power)
	ntm:commit("wireless")
	--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
	luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
	local v = {strength=power} 
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_timing()
	local r = true
	local starttime = luci.http.formvalue("starttime")              
	local endtime = luci.http.formvalue("endtime")              
	local starttimeh, starttimem = starttime:match("(%d+):(%d+)") 
	local endtimeh, endtimem = endtime:match("(%d+):(%d+)") 
	luci.http.prepare_content("application/json")
	if starttimeh and starttimem and endtimeh and endtimem then
		--luci.http.write_json(starttimeh)
		--luci.http.write_json(starttimem)
		--luci.http.write_json(endtimeh)
		--luci.http.write_json(endtimem)
		if starttimeh == endtimeh and starttimem == endtimem then
			r = false
		else
		luci.sys.call([[sed /"sbin\/wifi"/d -i /etc/crontabs/root]])
		luci.sys.call([[echo "]] .. starttimem .. [[ ]] .. starttimeh .. [[ * * * /sbin/wifi down" >> /etc/crontabs/root]])
		luci.sys.call([[echo "]] .. endtimem .. [[ ]] .. endtimeh .. [[ * * * /sbin/wifi up" >> /etc/crontabs/root]])
		end
	else
		r = false
	end
	local s = {result=r,value = v}
	luci.http.write_json(s)
	
end

function myfun_cleartiming()
	luci.sys.call([[sed /"sbin\/wifi"/d -i /etc/crontabs/root]])
	luci.http.prepare_content("application/json")                           
	local r = true
	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_gettiming()
	local r = true
	local v = {}
	local starttime
	local endtime
	local starttimem = luci.sys.exec([[cat /etc/crontabs/root | grep "wifi down" | awk '{printf $1}']])
	local starttimeh = luci.sys.exec([[cat /etc/crontabs/root | grep "wifi down" | awk '{printf $2}']])
	local endtimem = luci.sys.exec([[cat /etc/crontabs/root | grep "wifi up" | awk '{printf $1}']])
	local endtimeh = luci.sys.exec([[cat /etc/crontabs/root | grep "wifi up" | awk '{printf $2}']])
	--luci.http.write_json(starttimeh)
	--luci.http.write_json(starttimem)
	--luci.http.write_json(endtimeh)
	--luci.http.write_json(endtimem)
	if starttimeh and starttimem and endtimeh and endtimem 
		and starttimem ~= "" or starttimeh ~= "" or endtimem ~= "" or endtimeh ~= "" then
		starttime = starttimeh .. ":" .. starttimem
		endtime = endtimeh .. ":" .. endtimem
		v = {starttime = starttime, endtime = endtime}
	else
		luci.sys.call([[sed /"sbin\/wifi"/d -i /etc/crontabs/root]])
		v = {starttime = "", endtime = ""}
	end

	local s = {result=r,value = v}
	luci.http.write_json(s)
end

function myfun_unlink()
	local maclist
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	local mac = luci.http.formvalue("mac")              
	local change = false
	local r = false
	luci.http.prepare_content("application/json")                           
	if mac then
		for _, net in ipairs(dev:get_wifinets()) do
			if net:get("mode") == "ap" then
				maclist = net:get("maclist")
				--luci.http.write_json(maclist)
				net:set("macfilter", "deny")
				if maclist == nil then
					maclist = {}
				end
				change = true
				for _, amac in ipairs(maclist) do
					if amac == mac then
						change = false --mac地址已经被过滤
					end
				end
				if change then
					maclist[#maclist + 1] = mac
					net:set("maclist", maclist)
				end
				r = true
				break
			end
		end
	end
	if mac then	
		local s = require "luci.tools.status"
		local rv = {}
		local dhcplease = s.dhcp_leases()
		for _,dev in pairs(dhcplease) do
			if dev.macaddr == mac then
				local ip = dev.ipaddr
				luci.sys.call([[echo ]] .. dev.ipaddr .. [[ > /tmp/outline]])
			end
		end
		
	end 

	if maclist == nil then
		maclist = {}
	end
	local s = {result=r,value = maclist}
	luci.http.write_json(s)
	
	if change then
		ntm:commit("wireless")
		luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
	end
end

function myfun_link()
	local maclist
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	local mac = luci.http.formvalue("mac")              
	local r = false
	local change = false
	luci.http.prepare_content("application/json")                           
	if mac then
		for _, net in ipairs(dev:get_wifinets()) do
			if net:get("mode") == "ap" then
				maclist = net:get("maclist")
				--luci.http.write_json(maclist)
				if maclist == nil then
					break
				end
				for pos, amac in ipairs(maclist) do
					if amac == mac then
						table.remove(maclist, pos)
						r = true
						if #maclist == 0 then
							net:set("maclist", nil)
							net:set("macfilter", nil)
						else
							net:set("maclist", maclist)
						end
						change = true
					end
				end
				break
			end
		end
	end
	if maclist == nil then
		maclist = {}
	end
	local s = {result=r,value = maclist}
	luci.http.write_json(s)
	--luci.http.write_json(maclist)
	if change then
		ntm:commit("wireless")
		--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
		luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
	end
end

function myfun_alllink()
	local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		if net:get("mode") == "ap" then
			net:set("maclist", nil)
			net:set("macfilter", nil)
			ntm:commit("wireless")
			--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
			luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
			break
		end
	end
	local r = true
	local s = {result=r,value = {}}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)

	
end

function myfun_unlinklist()
	local maclist
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	local mac = luci.http.formvalue("mac")              
	local r = false
	luci.http.prepare_content("application/json")                           
	for _, net in ipairs(dev:get_wifinets()) do
		if net:get("mode") == "ap" then
			maclist = net:get("maclist")
			r = true
			--luci.http.write_json(maclist)
		end
	end
	
	if maclist == nil then
		maclist = {}
	end
	local rv = {}
	for pos, amac in ipairs(maclist) do
		local rd =
		{
			mac = amac,
		}
		rd.hostname = luci.sys.exec([[grep ]] .. rd.mac .. [[ /etc/dhcp.leases -i | awk '{printf $3}']])
		rv[#rv+1] = rd
	end
	local s = {result=r,value = rv}
	luci.http.write_json(s)
end

function myfun_wlanjoin()
	local dev = luci.http.formvalue("device")              
        local ntm = require "luci.model.network".init()
	luci.http.prepare_content("application/json")                           
	
	local wnet = ntm:get_wifinet("wlan0-1")
	luci.http.write_json(wnet)

	local wdev = ntm:get_wifidev("radio0")
	luci.http.write_json(wdev)

	luci.http.write("###################\n")
        for _, dev in ipairs(ntm:get_wifidevs()) do
		luci.http.write_json(dev)
		luci.http.write("\n")
		for _, net in ipairs(dev:get_wifinets()) do
			luci.http.write("\n")
			luci.http.write_json(net)
			luci.http.write("\n")
		end
		luci.http.write("\n")
	end
	
end

function myfun_get_wandev()                                                                      
        local ntm = require "luci.model.network".init()
	local dev = ntm:get_wandev()
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(dev)
	luci.http.write("\n###################\n")
	local interface = ntm:get_interface("eth0")
	luci.http.write_json(interface)
	interface = ntm:get_interface("eth0")
	luci.http.write_json(interface)
	interface = ntm:get_interface("br-lan")
	luci.http.write_json(interface)

	luci.http.write("\n###################\n")
	local protocols = ntm:get_protocols()
	local networks = ntm:get_networks()
	local interfaces = ntm:get_interfaces()
	for _, net in ipairs(interfaces) do
		luci.http.write_json(net:name())
		luci.http.write_json(net:get_networks())
		
	end
	luci.http.write("\n###################\n")
	for _, net in ipairs(networks) do
		luci.http.write_json(net:uptime())
		luci.http.write_json(net:name())
		luci.http.write_json(net:proto())
		
	end
	luci.http.write("\n###################\n")
	for _, net in ipairs(protocols) do
		luci.http.write_json(net:name())
		luci.http.write_json(net:ifname())
		luci.http.write_json(net:uptime())
		luci.http.write_json(net:ipaddr())
		luci.http.write_json(net:get_interfaces())
		
	end
	
	

end 

