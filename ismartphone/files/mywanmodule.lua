require("luci.json")
require("luci.model.uci")
require("luci.model.network")
require("luci.sys")
module("luci.controller.myapp.mywanmodule", package.seeall)

function index()
	entry({"net"}, firstchild(), _("net"), 89)
	entry({"net", "wan"}, firstchild(), _("netwan"), 89)
	entry({"net", "wan", "myrouterstatus"}, call("myfun_myrouterstatus"), _("myrouterstatus"), 89)
	entry({"net", "wan", "wanstatus"}, call("myfun_wanstatus"), _("wanstatus"), 89)
	entry({"net", "wan", "running"}, call("myfun_running"), _("running"), 89)
	entry({"net", "wan", "abnormal"}, call("myfun_abnormal"), _("abnormal"), 89)
--	entry({"net", "wan", "tointernet"}, call("myfun_tointernet"), _("tointernet"), 89)
	entry({"net", "wan", "inoutbound"}, call("myfun_inoutbound"), _("inoutbound"), 89)
	entry({"net", "wan", "dhcpclient"}, call("myfun_dhcpclient"), _("dhcpclient"), 89)
	entry({"net", "wan", "staticaddress"}, call("myfun_staticaddress"), _("staticaddress"), 90)
	entry({"net", "wan", "pppoe"}, call("myfun_pppoe"), _("pppoe"), 90)
	entry({"net", "wan", "repeater"}, call("myfun_repeater"), _("repeater"), 90)
	entry({"net", "wan", "internet"}, call("myfun_internet"), _("internet"), 90)
	entry({"net", "wan", "diagnostics"}, call("myfun_diagnostics"), _("diagnostics"), 90)
	entry({"net", "wan", "totalflow"}, call("myfun_totalflow"), _("totalflow"), 90)
	entry({"net", "wan", "showparams"}, call("myfun_showparams"), _("showparams"), 90)
end

function myfun_myrouterstatus()
	local r = true
	local uci = luci.model.uci.cursor()
	local wifiable = uci:get("network", "wireless", "disabled")
	local ssid
	local channel
	local inbound = 0
	local outbound = 0
	local hostnametotal = 0
	local smartnodetotal
	local mouthflow = 0
	if wifiable == 1 then
		wifiable = 0
	else
		wifiable = 1
	end
	local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		if net:get("mode") == "ap" then
			ssid = net:get("ssid")
			channel = net:channel();
			break
		end
	end

	local proto = uci:get("network", "wan", "proto")
	local interface
	if proto == "repeater" then
		interface = "wlan0-16"
	else
		interface = "eth0"
	end

	--luci.sys.exec([[vnstatflow -ru 0 -tr 2 -i ]] .. interface .. [[ > /var/vnstat-tr]])
	--inbound = luci.sys.exec([[cat /var/vnstat-tr | grep "rx" | awk '{printf $2}' ]])
	--outbound = luci.sys.exec([[cat /var/vnstat-tr | grep "tx" | awk '{printf $2}' ]])
	inbound = luci.sys.exec([[cat /tmp/online | awk '{sum+=$3}END{printf sum}']])
	inbound = inbound / 1000
	outbound = luci.sys.exec([[cat /tmp/online | awk '{sum+=$2}END{printf sum}']])
	outbound = outbound / 1000

	--local s = require "luci.tools.status"
	--local dhcplease = s.dhcp_leases()
	--for _,dev in pairs(dhcplease) do
	--	hostnametotal = hostnametotal + 1
	--end
	hostnametotal = luci.sys.exec([[wc -l /tmp/online | awk '{printf $1}']])

	smartnodetotal = 0

	local mouthflow1 = luci.sys.exec([[vnstatflow -m -i eth0 | awk '/rx/{getline;getline;printf $3}']])
	local mouthflow2 = luci.sys.exec([[vnstatflow -m -i wlan0-16 | awk '/rx/{getline;getline;printf $3}']])
	--luci.http.write_json(mouthflow1)
	--luci.http.write_json(mouthflow2)
	if mouthflow1 and mouthflow1 ~= "" then
		mouthflow = mouthflow + tonumber(mouthflow1)
	end
	if mouthflow2 and mouthflow2 ~= "" then
		mouthflow = mouthflow + tonumber(mouthflow2)
	end

	local v = {wifiable = wifiable, ssid=ssid, channel = channel, inbound = inbound, outbound = outbound,
				hostnametotal = hostnametotal, smartnodetotal = smartnodetotal, mouthflow = mouthflow} 
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
end

function myfun_wanstatus()
	local uci = luci.model.uci.cursor()
	local ntm = require "luci.model.network".init()
	local r = true
	--local phystatus = luci.sys.call("ifconfig eth0 | grep RUNNING > /dev/null")
	local proto = uci:get("network", "wan", "proto")
	--local address = luci.sys.exec([[ifconfig eth0 | grep "inet addr" | awk -F: '{print $2}' | awk '{printf $1}']])
	--local netmask =	luci.sys.exec([[ifconfig eth0 | grep "Mask" | awk -F: '{print $4}' | awk '{printf $1}']])
	--local gateway = luci.sys.exec([[ip route get 8.8.8.8 | awk '{printf $3}']])
	--local dns = luci.sys.exec([[awk '/ wan/{getline; printf $NF}' /tmp/resolv.conf.auto]])
	--local proto = wannet:proto()
	local phystatus
	local address
	local netmask
	local gwaddr
	local dnsaddrs
	if proto == "repeater" then
		local wwannet = ntm:get_network("wwan")
		address = wwannet:ipaddr()
		netmask = wwannet:netmask()
		gwaddr = wwannet:gwaddr()
		dnsaddrs = wwannet:dnsaddrs()
		phystatus = wwannet:uptime()
	else
		local wannet = ntm:get_network("wan")
		address = wannet:ipaddr()
		netmask = wannet:netmask()
		gwaddr = wannet:gwaddr()
		dnsaddrs = wannet:dnsaddrs()
		phystatus = wannet:uptime()
	end
	local v = {status=phystatus,type=proto,address=address,netmask=netmask,gateway=gateway,dns=dns} 
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_running()
	local r = true
	local phystatus = luci.sys.call("ifconfig eth0 | grep RUNNING > /dev/null")
	local v = {running = phystatus}
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
end

function myfun_abnormal()
	local r = true
	local err = 0
	local message
	local uci = luci.model.uci.cursor()
	local proto = uci:get("network", "wan", "proto")
	local v = {}
	v.internet = proto
	if proto ~= "repeater" then
		local phystatus = luci.sys.call("ifconfig eth0 | grep RUNNING > /dev/null")
		if phystatus ~=0 then
			err = 4
			message = "wan网线未插好"
		end
	end
	if proto == "static" then
		v.ipaddr = uci:get("network", "wan", "ipaddr")
		v.netmask = uci:get("network", "wan", "netmask")
		v.gateway = uci:get("network", "wan", "gateway")
		v.dns = uci:get("network", "wan", "dns")
		if v.ipaddr == nil or v.ipaddr == "" or v.network == nil or v.network == ""
			or v.gateway == nil or v.gateway == "" or v.dns == nil or v.dns == "" then
			err = 1
			message = "静态地址配置不正确，请重新配置"
		end
	elseif proto == "pppoe" then
		v.username = uci:get("network", "wan", "username")
		v.password = uci:get("network", "wan", "password")
		if v.username == nil or v.username == "" or v.password == nil or v.password == "" then
			err = 2
			message = "拨号配置不正确，请重新配置"
		end
	elseif proto == "repeater" then
		
		local ntm = require "luci.model.network".init()
		local dev = ntm:get_wifidev("radio0")
		local flag = false
		for _, net in ipairs(dev:get_wifinets()) do
			if net:mode() == "sta" then
				v.ssid = net:ssid()
				v.bssid = net:bssid()
				v.key = net:get("key")
				flag = true
				break
			end
		end
		if flag == false then
			err = 5
			message = "中继配置不正确，请重新配置"
		end
	elseif proto == "dhcp" then
	else
		err = 3
		message = "网络配置异常，请重新配置网络"
	end

	v = {err = err, message = message}
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
end

--function myfun_tointernet()
--	local ping = luci.sys.exec([[ping www.baidu.com -c 4 | grep "100%"]])
--end

function myfun_inoutbound()
	local uci = luci.model.uci.cursor()
	local proto = uci:get("network", "wan", "proto")
	local r = true
	local interface
	if proto == "repeater" then
		interface = "wlan0-16"
	else
		interface = "eth0"
	end

	luci.sys.exec([[vnstat -tr 2 -i ]] .. interface .. [[ > /var/vnstat-tr]])
	local inbound = luci.sys.exec([[cat /var/vnstat-tr | grep "rx" | awk '{printf $2$3}' ]])
	local outbound = luci.sys.exec([[cat /var/vnstat-tr | grep "tx" | awk '{printf $2$3}' ]])
	local v = {inbound=inbound,outbound=outbound} 
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_dhcpclient()
	local uci = luci.model.uci.cursor()
	uci:set("network","wan","proto","dhcp")
	local r = uci:commit("network")
	--uci:apply("network")
	
	local flagwifi = false
	local ntm = require "luci.model.network".init()
	local dev = ntm:get_wifidev("radio0")
	for _, net in ipairs(dev:get_wifinets()) do
		if net:mode() == "sta" then
			dev:del_wifinet(net) --无线网卡不再工作在client(sta)模式
			flagwifi = true
		end
	end
	if flagwifi == true then
		ntm:commit("wireless")
		--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
		luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
	end

	luci.sys.call("(env -i /sbin/ifdown wan; env -i /sbin/ifup wan) >/dev/null 2>/dev/null")
	local v = nil 
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_staticaddress()
	local r = false
	local flagwifi = false
	local uci = luci.model.uci.cursor()
	local ipaddr = luci.http.formvalue("ipaddr")
	local netmask = luci.http.formvalue("netmask")
	local gateway = luci.http.formvalue("gateway")
	local dns = luci.http.formvalue("dns")
	if ipaddr and netmask then
		uci:set("network","wan","ipaddr",ipaddr)
		uci:set("network","wan","netmask",netmask)
		uci:set("network","wan","gateway",gateway)
		uci:set("network","wan","dns",dns)
		uci:set("network","wan","proto","static")
		r = uci:commit("network")
		--uci:apply("network")
		local ntm = require "luci.model.network".init()
		local dev = ntm:get_wifidev("radio0")
		for _, net in ipairs(dev:get_wifinets()) do
			if net:mode() == "sta" then
				dev:del_wifinet(net) --无线网卡不再工作在client模式
				flagwifi = true
			end
		end
		if flagwifi == true then
			ntm:commit("wireless")
			luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
		end
		luci.sys.call("(env -i /sbin/ifdown wan; env -i /sbin/ifup wan) >/dev/null 2>/dev/null")
	end	
	
	local v = nil 
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_pppoe()
	local r = false
	local uci = luci.model.uci.cursor()
	local username = luci.http.formvalue("username")
	local password = luci.http.formvalue("password")
	local flagwifi = false
	if username and password then
		uci:set("network", "wan", "username", username)
		uci:set("network", "wan", "password", password)
		uci:set("network", "wan", "proto", "pppoe")
		r = uci:commit("network")
		--uci:apply("network")
		local ntm = require "luci.model.network".init()
		local dev = ntm:get_wifidev("radio0")
		for _, net in ipairs(dev:get_wifinets()) do
			if net:mode() == "sta" then
				dev:del_wifinet(net) --无线网卡不再工作在client模式
				flagwifi = true
			end
		end
		if flagwifi == true then
			ntm:commit("wireless")
			luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
		end
		luci.sys.call("(env -i /sbin/ifdown wan; env -i /sbin/ifup wan) >/dev/null 2>/dev/null")
	end
	
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_repeater()
	local r = false
	local uci = luci.model.uci.cursor()
	local bssid = luci.http.formvalue("bssid")
	local key = luci.http.formvalue("key")
	local sta
	
	if bssid then
		local iw = luci.sys.wifi.getiwinfo("radio0")
		local ntm = require "luci.model.network".init()
		local dev = ntm:get_wifidev("radio0")
		for k, v in ipairs(iw.scanlist) do
			if bssid == v.bssid then
				if v.encryption.wpa ~= 0 and key == nil then
					break --已经加密,但是密码为空
				end
				for _, net in ipairs(dev:get_wifinets()) do
					if net:mode() == "sta" then
						dev:del_wifinet(net)
					end
				end
				local encryption
				if v.encryption.wpa == 0 then
					encryption = "none"
				elseif v.encryption.wpa == 1 then
					encryption = "psk"
				elseif v.encryption.wpa == 2 then
					encryption = "psk2"
				elseif v.encryption.wpa == 3 then
					encryption = "psk-mixed"
				end
				local wconf = {                                                                               
					device  = "radio0",                                                            
					ssid    = v.ssid,                                                              
					mode    = "sta",                         
					network = "wwan",
					bssid   = bssid,
					key     = key,
					encryption = encryption,
					ifname = "wlan0-16",
				}
				dev:add_wifinet(wconf)
				ntm:commit("wireless")
				ntm:add_network("wwan", {proto="dhcp"})
				ntm:commit("network")

				local fs = require "nixio.fs"
				local has_firewall = fs.access("/etc/config/firewall")
				local fwm = require "luci.model.firewall".init()
				if has_firewall then
					local wanzone = fwm:get_zone("wan")
					local wannetwork = wanzone:get_networks()
					if wanzone then
						wanzone:add_network("wwan")
						fwm:commit("firewall")
					end
				end
				
				--luci.sys.call("(env -i /sbin/wifi down; env -i /sbin/wifi up) >/dev/null 2>/dev/null")
				luci.sys.call("(sleep 1 && /sbin/wifi &) >/dev/null 2>/dev/null")
				uci:set("network","wan","proto","repeater")
				r = uci:commit("network")
				--uci:apply("network")
				luci.sys.call("(env -i /sbin/ifdown wan; env -i /sbin/ifup wan) >/dev/null 2>/dev/null")
				break
			end
		end
	end


	luci.http.prepare_content("application/json")                           
	local v = nil 
	local s = {result=r,value=v}
	luci.http.write_json(s)
end

function myfun_internet()
	local r = true
	local v = {}
	local uci = luci.model.uci.cursor()
	local proto = uci:get("network", "wan", "proto")
	v.internet = proto
	if proto == "static" then
		v.ipaddr = uci:get("network", "wan", "ipaddr")
		v.netmask = uci:get("network", "wan", "netmask")
		v.gateway = uci:get("network", "wan", "gateway")
		v.dns = uci:get("network", "wan", "dns")
	elseif proto == "pppoe" then
		v.username = uci:get("network", "wan", "username")
		v.password = uci:get("network", "wan", "password")
	elseif proto == "repeater" then
		
		local ntm = require "luci.model.network".init()
		local dev = ntm:get_wifidev("radio0")
		for _, net in ipairs(dev:get_wifinets()) do
			if net:mode() == "sta" then
				v.ssid = net:ssid()
				v.bssid = net:bssid()
				v.key = net:get("key")
			break
			end
		end
	end
	luci.http.prepare_content("application/json")                           
	local s = {result=r,value=v}
	luci.http.write_json(s)
end

function myfun_diagnostics()
	local p = luci.http.context.request.message.params
	
	local r=true
	local pingresult
	if p.website == nil then
		r = false
	else
		pingresult = luci.sys.net.pingtest(p.website)	
	end
	local v = {ping=pingresult} 
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
	
end

function myfun_totalflow()
	local r=true
	local mouthflow = 0
	local one = 0
	local two = 0
	local three = 0
	local four = 0
	local five = 0
	local six = 0
	local seven = 0
	local mouthflow1 = luci.sys.exec([[vnstatflow -m -i eth0 | awk '/rx/{getline;getline;printf $3}']])
	local mouthflow2 = luci.sys.exec([[vnstatflow -m -i wlan0-16 | awk '/rx/{getline;getline;printf $3}']])
	if mouthflow1 and mouthflow1 ~= "" then
		mouthflow = mouthflow + tonumber(mouthflow1)
	end
	if mouthflow2 and mouthflow2 ~= "" then
		mouthflow = mouthflow + tonumber(mouthflow2)
	end
	local one1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '2p' | awk '{printf $1}']])
	local one2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '2p' | awk '{printf $1}']])
	if one1 and one1 ~= "" then
		one = one + tonumber(one1)
	end
	if one2 and one2 ~= "" then
		one = one + tonumber(one2)
	end
	local two1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '3p' | awk '{printf $1}']])
	local two2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '3p' | awk '{printf $1}']])
	if two1 and two1 ~= "" then
		two = two + tonumber(two1)
	end
	if two2 and two2 ~= "" then
		two = two + tonumber(two2)
	end
	local three1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '4p' | awk '{printf $1}']])
	local three2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '4p' | awk '{printf $1}']])
	if three1 and three1 ~= "" then
		three = three + tonumber(three1)
	end
	if three2 and three2 ~= "" then
		three = three + tonumber(three2)
	end
	local four1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '5p' | awk '{printf $1}']])
	local four2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '5p' | awk '{printf $1}']])
	if four1 and four1 ~= "" then
		four = four + tonumber(four1)
	end
	if one2 and one2 ~= "" then
		four = four + tonumber(four2)
	end
	local five1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '6p' | awk '{printf $1}']])
	local five2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '6p' | awk '{printf $1}']])
	if five1 and five1 ~= "" then
		five = five + tonumber(five1)
	end
	if five2 and five2 ~= "" then
		five = five + tonumber(five2)
	end
	local six1 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '7p' | awk '{printf $1}']])
	local six2 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '7p' | awk '{printf $1}']])
	if six1 and six1 ~= "" then
		six = six + tonumber(six1)
	end
	if six2 and six2 ~= "" then
		six = six + tonumber(six2)
	end
	local seven1 = luci.sys.exec([[vnstatflow -i eth0 -7 | sed -n '8p' | awk '{printf $1}']])
	local seven2 = luci.sys.exec([[vnstatflow -i wlan0-16 -7 | sed -n '8p' | awk '{printf $1}']])
	if seven1 and seven1 ~= "" then
		seven = seven + tonumber(seven1)
	end
	if seven2 and seven2 ~= "" then
		seven = seven + tonumber(seven2)
	end

	local v = {mouthflow=mouthflow, one = one, two=two,three=three,four=four,five=five,six=six,seven=seven} 
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
	
end

function myfun_showparams()
	local p = luci.http.context.request.message.params
	luci.http.prepare_content("text/plain")
	luci.http.write_json(p)                                                

	--[[
	for i,v in pairs(p) do
		luci.http.write(i)
		luci.http.write(":")
		luci.http.write(v)
		luci.http.write("\n")
	end
	p =  luci.http.formvalue("website")
	local j = luci.json.encode(p)
	luci.http.write(j)
	]]--
end

