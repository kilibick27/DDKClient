// This file can be included several times.

#ifndef CONFIG_DOMAIN
#error "CONFIG_DOMAIN macro not defined"
#define CONFIG_DOMAIN(Name, ConfigPath, HasVars) ;
#endif

CONFIG_DOMAIN(DDNET, "settings_ddnet.cfg", true)
CONFIG_DOMAIN(TCLIENT, "settings_ddkclient.cfg", true)
CONFIG_DOMAIN(TCLIENTPROFILES, "ddkclient_profiles.cfg", false)
CONFIG_DOMAIN(TCLIENTCHATBINDS, "ddkclient_chatbinds.cfg", false)
CONFIG_DOMAIN(TCLIENTWARLIST, "ddkclient_warlist.cfg", false)
CONFIG_DOMAIN(TCLIENTDUMMY, "ddkclient_dummy.cfg", true)
