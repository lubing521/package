require("luci.json")
require("luci.model.uci")
require("luci.sys")
module("luci.controller.myapp.mysysmodule", package.seeall)
function index()
	entry({"node"}, firstchild(), _("system"), 89)
	entry({"node", "status"}, call("myfun_nodestatus"), _("nodestatus"), 89)
	entry({"system"}, firstchild(), _("system"), 89)
	entry({"system", "isguogee"}, call("myfun_isguogee"), _("isguogee"), 89)
	entry({"system", "checklogin"}, call("myfun_checklogin"), _("checklogin"), 89)
	entry({"system", "reboot"}, call("myfun_reboot"), _("reboot"), 89)
	entry({"system", "shutdown"}, call("myfun_shutdown"), _("shutdown"), 89)
	entry({"system", "uptime"}, call("myfun_uptime"), _("uptime"), 89)
	entry({"system", "changepassword"}, call("myfun_changepassword"), _("changepassword"), 89)
	entry({"system", "passwordsecurity"}, call("myfun_passwordsecurity"), _("passwordsecurity"), 89)
	entry({"system", "lightonoff"}, call("myfun_lightonoff"), _("lightonoff"), 89)
	entry({"system", "lightstatus"}, call("myfun_lightstatus"), _("lightstatus"), 89)
end

function myfun_isguogee()
	local r = true
	local mac = luci.sys.exec([[ifconfig br-lan | grep HWaddr | awk '{printf $5}']])
	local v = {mac = mac}
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
	
end


function myfun_checklogin()
	--local dsp = require "luci.dispatcher"
	--local sauth = require "luci.sauth"
	luci.http.prepare_content("application/json")                           
	local user = luci.http.formvalue("username")
	local pass = luci.http.formvalue("userpwd")
	local r = true
	local check = false
	local message
	if user and pass then
		check = luci.sys.user.checkpasswd(user, pass)
	end
	if check == true then
		check = 0
		message = "success"
	else
		check = 2
		message = "err"
	end
	local v = {err = check, message = message}
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	s = luci.http.getcookie("sysauth")
--	luci.http.write_json(s)
	--luci.http.header("Set-Cookie", "sysauth=" .. "HELLO123WORLD".."; path=/cgi-bin/luci/system/update")
--luci.http.header("Set-Cookie", "sysauth=; path=/cgi-bin/luci/")
--local dsp = require "luci.dispatcher"  
--        local sauth = require "luci.sauth"              
--        if dsp.context.authsession then                                           
--                sauth.kill(dsp.context.authsession)                               
--                dsp.context.urltoken.stok = nil                                   
--        end                                                                       
                                                                          
--        luci.http.header("Set-Cookie", "sysauth=; path=" .. dsp.build_url())
--        luci.http.redirect(luci.dispatcher.build_url())
end

function myfun_reboot()
	local r = luci.sys.reboot()
	if r == 0 then
		r = true
	end
	local v = nil
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_shutdown()
	local r = luci.sys.call("halt")
	if r == 0 then
		r = true
	end
	local v = nil
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_uptime()
	local r
	local uptime = luci.sys.uptime()
	if uptime > 0 then
		r = true
	end
	v = {uptime=uptime}
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_changepassword()
	local r = true
	local olduserpwd = luci.http.formvalue("olduserpwd")
	local userpwd = luci.http.formvalue("userpwd")
	local security = 4
	if userpwd == nil or userpwd == "" or olduserpwd == "" or olduserpwd == nil then
		security = 2
	else
		--local result = luci.sys.call("/usr/lib/lua/luci/controller/myapp/chpass.sh " .. p.userpwd)
		local check
		check = luci.sys.user.checkpasswd("root", olduserpwd)
		if userpwd == "admin" then
			security = 1
		elseif #userpwd < 6 then
			security = 5
		elseif check then
			local result = luci.sys.user.setpasswd("root", userpwd)  --lua官方api，不用上面自己写的
			if result ~= 0 then
				security = 6
			else
				security = 0
			end
		end
	end

	v = {security = security}
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_passwordsecurity()
	local r = true
	local security = 0 --安全
	local tmp = luci.sys.user.getpasswd("root")
	if tmp == nil then
		security = 2  --没有密码
	else
		local tmp = luci.sys.user.checkpasswd("root", "admin")
		if tmp == true then
			security = 1 --默认密码
		else
			local tmp = luci.sys.user.checkpasswd("root", "")
			if tmp == true then
				security = 3 --有密码，但是密码为空字符串
			end
		end
	end
	local v = {security = security}
	local s = {result=r,value=v}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_nodestatus()
	
	local rv = {}
	local rd = 
	{
		type = 0x10,
		count = 0,
	}
	rv[#rv+1] = rd
	local rd = 
	{
		type = 0x20,
		count = 0,
	}
	rv[#rv+1] = rd
	local rd = 
	{
		type = 0x30,
		count = 0,
	}
	rv[#rv+1] = rd
	local rd = 
	{
		type = 0x40,
		count = 0,
	}
	rv[#rv+1] = rd
	local rd = 
	{
		type = 0x41,
		count = 0,
	}
	rv[#rv+1] = rd
	local rd = 
	{
		type = 0x50,
		count = 0,
	}
	rv[#rv+1] = rd
	local r = true
	local s = {result=r,value=rv}
	local j = luci.json.encode(s)
	luci.http.prepare_content("text/plain")
	luci.http.write(j)
end

function myfun_lightonoff()
	local onoff = luci.http.formvalue("onoff")
	local r = false
	local v
	if onoff == "0" or onoff == "1" then
		r = true
		v = onoff
		luci.sys.call([[echo ]] .. onoff .. [[ > /etc/lightonoff]])
	end
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
	
end

function myfun_lightstatus()
	local r = false
	local v
	local onoff = luci.sys.exec([[head -c 1 /etc/lightonoff]])
	if onoff == "0" or onoff == "1" then
		r = true
		v = onoff
	else
		r = true
		v = "0"
	end
	local s = {result=r,value=v}
	luci.http.prepare_content("application/json")                           
	luci.http.write_json(s)
end

