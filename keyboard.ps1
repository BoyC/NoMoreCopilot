Get-PnpDevice -Class Keyboard -PresentOnly |
  ForEach-Object {
    $_
    Get-PnpDeviceProperty -InstanceId $_.InstanceId DEVPKEY_Device_HardwareIds |
      Where-Object {$_.Data -match '^HID\\VID_'}
  }