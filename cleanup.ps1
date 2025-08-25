pnputil /enum-drivers | Select-String -Pattern NoMoreCopilot -Context 2
pnputil /delete-driver oemXX.inf /uninstall /force