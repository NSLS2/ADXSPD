# Standalone Sphinx config for previewing the ADXSPD docs outside of the areaDetector docs build.
# Kept out of docs/ so it isn't merged into the areaDetector docs tree.

project = "ADXSPD"
author = "Jakub Wlodek"
copyright = "2026, Brookhaven National Laboratory"

extensions = ["linuxdoc.rstFlatTable"]

root_doc = "ADXSPD/ADXSPD"
source_suffix = {".rst": "restructuredtext"}
exclude_patterns = ["_build"]

pygments_style = "sphinx"
html_theme = "sphinx_rtd_theme"
html_title = "ADXSPD"
