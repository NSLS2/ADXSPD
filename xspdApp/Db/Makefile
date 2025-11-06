TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------
#  Add MACRO DEFINITION AFTER THIS LINE

#----------------------------------
#  Optimization of db files using dbst (default:NO)
#DB_OPT = YES

#-------------------------------------------
# Create and install (or just install)
# databases, templates, subtitutions
DB += ADXSPD.template
DB += ADXSPDModule.template
DB += ADXSPD_settings.req

#-------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>
include $(TOP)/configure/RULES
#----------------------------------------------
# ADD Rules after tis line.
