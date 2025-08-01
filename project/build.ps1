$cores = [System.Environment]::ProcessorCount
scons --board=sf32lb52-yyds --board_search_path=../boards -j($cores)
.\build_sf32lb52-yyds_hcpu\uart_download.bat