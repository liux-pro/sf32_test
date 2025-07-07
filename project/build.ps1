$cores = [System.Environment]::ProcessorCount
scons --board=sf32lb52-lchspi-ulp -j($cores)
.\build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat