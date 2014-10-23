require("luci.json")
require("luci.model.uci")
require("luci.sys")
module("luci.controller.myapp.myportal", package.seeall)
function index()
	entry({"login"}, call("myfun_login"), _("login"), 89)
	entry({"portal"}, call("myfun_portal"), _("portal"), 89)
	entry({"auth"}, call("myfun_auth"), _("auth"), 89)
	entry({"auth", "index.php"}, call("myfun_auth"), _("indexphp"), 89)
	entry({"phpauth"}, call("myfun_auth"), _("phpauth"), 89)
	entry({"phpauth", "auth"}, call("myfun_auth"), _("phpauthauth"), 89)
	entry({"phpauth", "auth", "index.php"}, call("myfun_auth"), _("indexphp"), 89)
	entry({"ping"}, call("myfun_ping"), _("auth"), 89)
end

function myfun_login()
	local v = "haha! you must portal first!!"
	luci.http.prepare_content("text/plain")
	luci.http.write(v)
end

function myfun_portal()
	local v = "haha! you auth success,portal end"
	luci.http.prepare_content("text/plain")
	luci.http.write(v)
end

function myfun_auth()
	luci.http.prepare_content("text/plain")
	luci.http.write("Auth: 1")
	local p = luci.http.context.request.message.params
	for i,v in pairs(p) do
		luci.sys.call(" echo " .. i .. " : " .. v .. ">> /var/authparams")
	end
end

function myfun_ping()
	luci.http.write("Pong")
end
