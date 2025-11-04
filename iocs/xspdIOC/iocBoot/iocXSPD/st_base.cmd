
errlogInit(20000)

< envPaths
# < /epics/common/localhost-netsetup.cmd

epicsEnvSet("ENGINEER",                 "Jakub Wlodek")
epicsEnvSet("PORT",                     "XSPD1")
epicsEnvSet("IOC",                      "iocXSPD")
epicsEnvSet("PREFIX",                   "DEV:XSPD1:")
epicsEnvSet("HOSTNAME",                 "localhost")
epicsEnvSet("IOCNAME",                  "XSPD")
epicsEnvSet("QSIZE",                    "30")
epicsEnvSet("NCHANS",                   "2048")
epicsEnvSet("XSIZE",                    "1556")
epicsEnvSet("YSIZE",                    "516")
epicsEnvSet("NELMT",                    "65536")
epicsEnvSet("NDTYPE",                   "Int16")
epicsEnvSet("NDFTVL",                   "SHORT")
epicsEnvSet("CBUFFS",                   "500")
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

dbLoadDatabase("$(ADXSPD)/iocs/xspdIOC/dbd/xspdApp.dbd")
xspdApp_registerRecordDeviceDriver(pdbbase)

# Create instance of ADXSPD driver, and pause to show connection messages
ADXSPDConfig("$(PORT)", "http://localhost:8008")
# epicsThreadSleep(3)

dbLoadRecords("$(ADXSPD)/db/ADXSPD.template","P=$(PREFIX),R=cam1:,PORT=$(PORT),ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADXSPD)/db/ADXSPDModule.template", "P=$(PREFIX), R=cam1:,PORT=$(PORT),MODULE=0,ADDR=0,TIMEOUT=1")

# Load all other plugins using commonPlugins.cmd
< $(ADCORE)/iocBoot/commonPlugins.cmd

# set_requestfile_path("$(ADXSPD)/xspdApp/Db")

iocInit()

# save things every thirty seconds
# create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")
