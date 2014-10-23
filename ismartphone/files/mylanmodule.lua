
require("luci.json")
require("luci.model.uci")
require("luci.model.network")
require("luci.sys")
require("nixio.fs")
module("luci.controller.myapp.mylanmodule", package.seeall)

function index()
	local net = entry({"net"}, firstchild(), _("net"), 89)
	--net.sysauth = "root"
	--net.sysauth_authenticator = "htmlauth"
	entry({"net", "lan"}, firstchild(), _("netwan"), 89)
	entry({"net", "lan", "alluser"}, call("myfun_alluser"), _("alluser"), 89)
	entry({"net", "lan", "speedlimit"}, call("myfun_speedlimit"), _("speedlimit"), 89)
	entry({"net", "lan", "stopspeedlimit"}, call("myfun_stopspeedlimit"), _("stopspeedlimit"), 89)
	entry({"net", "lan", "getspeedlimit"}, call("myfun_getspeedlimit"), _("getspeedlimit"), 89)
	entry({"net", "lan", "speedup"}, call("myfun_speedup"), _("speedup"), 89)
	entry({"net", "lan", "stopspeedup"}, call("myfun_stopspeedup"), _("stopspeedup"), 89)
end

local function online_common()
	local rv = { }
	local nfs = require "nixio.fs"
	local leasefile = "/tmp/online"

	local fd = io.open(leasefile, "r")
	if fd then
		while true do
			local ln = fd:read("*l")
			if not ln then
				break
			else
				local ip, outbound, inbound = ln:match("^(%S+) (%S+) (%S+)")
				if ip and outbound and inbound then
					rv[#rv+1] = {
						ipaddr   = ip,
						outbound = outbound,
						inbound = inbound,
					}
				end
			end
		end
		fd:close()
	end

	return rv
end
function myfun_alluser()
	luci.http.prepare_content("application/json")                           
	local ntm = require "luci.model.network".init()
	local s = require "luci.tools.status"
	local rv = {}
	local dhcplease = s.dhcp_leases()
	local online = online_common()
	for _,dev in pairs(online) do
		local rd = 
		{
			type = 0,
			hostname = 0,
			address = dev.ipaddr,
			txrate = dev.outbound / 1024,
			rxrate = dev.inbound / 1024,
			limit = 0,
			inbound = 0,
			oubound = 0,
		}
		
		rd.mac = luci.sys.exec([[cat /proc/net/arp | grep "]] .. dev.ipaddr .. [[" | awk '{printf $4}']])
		for _,dhcp in pairs(dhcplease) do
			if rd.address == dhcp.ipaddr then
				rd.hostname = dhcp.hostname
			end
		end
		if rd.hostname == 0 then
			rd.hostname = luci.sys.exec([[grep ]] .. dev.ipaddr .. [[ /etc/dhcp.leases | awk '{printf $1}']])
		end
		local uci = luci.model.uci.cursor()
		uci:foreach("qosv4", "qos_mac", 
		function(p)
			if rd.mac == p.limit_mac then
				rd.outbound = p.UPLOADC--uci:get("qosv4", p['.name'], "UPLOADC")
				rd.inbound = p.DOWNLOADC--uci:get("qosv4", p['.name'], "DOWNLOADC")
				rd.limit = p.enable--uci:get("qosv4", p['.name'], "enable")
			end
		end)
		
		if nil ~= ntm:get_wifinet("wlan0") then
			for k,wdev in pairs(ntm:get_wifinet("wlan0"):assoclist()) do
		--luci.http.write_json(k)
		--luci.http.write_json(dev.macaddr)
				if rd.mac == k:lower() then
					rd.type = 1
				end
				
			end
		end
		rv[#rv+1] = rd
	end
	--luci.http.write_json(rv)
	local r = true
	local s = {result=r,value=rv}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
end

function myfun_speedlimit()
	luci.http.prepare_content("application/json")                           
	local r = false
	local ip = luci.http.formvalue("ip")
	local inbound = luci.http.formvalue("inbound")
	local outbound = luci.http.formvalue("outbound")
	local mac = luci.sys.exec([[grep ]] .. ip .. [[ /proc/net/arp | awk '{printf $4}']])
	local uci = luci.model.uci.cursor()
	local flagnew = true
	if ip and ip ~= "" and mac and mac ~= "" and inbound and outbound then
		-- 如果配置文件不存在,自动创建
		if (not nixio.fs.access("/etc/config/qosv4")) then
			nixio.open("/etc/config/qosv4", "w+")
			uci:section("qosv4", "qos_settings", nil, {enable = "1"})
		end

		uci:foreach("qosv4", "qos_mac", 
		function(p)
			if mac == p.limit_mac then
				uci:set("qosv4", p['.name'], "UPLOADC", outbound)
				uci:set("qosv4", p['.name'], "DOWNLOADC", inbound)
				uci:set("qosv4", p['.name'], "enable", "1")
				flagnew=false
			end
		end)
		if flagnew then
			uci:section("qosv4", "qos_mac", nil, {
				limit_mac = mac,
				UPLOADC = outbound,
				DOWNLOADC = inbound,
				enable = "1"
			})
		end
		r = uci:commit("qosv4")
		luci.sys.call("(env -i /etc/init.d/ismartqos restart) >/dev/null 2>/dev/null")
	end
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_stopspeedlimit()
	
	luci.http.prepare_content("application/json")                           
	local r = false
	local ip = luci.http.formvalue("ip")
	local mac = luci.sys.exec([[grep ]] .. ip .. [[ /proc/net/arp | awk '{printf $4}']])
	local uci = luci.model.uci.cursor()
	if ip and ip ~= "" and mac and mac ~= "" then
		uci:foreach("qosv4", "qos_mac", 
		function(p)
			if mac == p.limit_mac then
				uci:delete("qosv4", p['.name'])
			end
		end)
		r = uci:commit("qosv4")
		luci.sys.call("(env -i /etc/init.d/ismartqos restart) >/dev/null 2>/dev/null")
	end
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

function myfun_getspeedlimit()
	luci.http.prepare_content("application/json")                           
	local r = false
	local rv = {}
	local uci = luci.model.uci.cursor()
	uci:foreach("qosv4", "qos_mac", 
	function(p)
		local rd = {
			mac = p.limit_mac,
			inbound = p.DOWNLOADC,
			outbound = p.UPLOADC,
			enable = p.enable,
		}
		rv[#rv+1] = rd
	end)
	r = true
	--luci.sys.call("(env -i /etc/init.d/ismartqos restart) >/dev/null 2>/dev/null")
	local s = {result=r,value=rv}
	luci.http.write_json(s)
end

function myfun_speedup()
	luci.http.prepare_content("application/json")                           
	local r = false
	local ip = luci.http.formvalue("ip")
	if ip then
		r = true
		os.execute([[qosrmat >/dev/null 2>&1]])
		os.execute([[qosjiasu ]] .. ip .. [[ > /dev/null 2>&1]])
		os.execute([[qosaddat >/dev/null 2>&1]])
		--os.execute([[/tmp/c.sh &]])
	end
	local s = {result=r,value=v}
	luci.http.write_json(s)
end

function myfun_stopspeedup()
	luci.http.prepare_content("application/json")                           
	local r = true
	luci.sys.call([[qosv4 stop > /dev/null 2>&1]])
	os.execute([[qosrmat >/dev/null 2>&1]])

	local s = {result=r,value=v}
	luci.http.write_json(s)
end

