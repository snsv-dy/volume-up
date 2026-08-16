# TODO: Add os/ to git. I zmień nazwę na tp

# Circular buffer:
https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/

Before we proceed, we should take a moment to discuss the method we will use to determine whether or buffer is full or empty.

Both the “full” and “empty” cases of the circular buffer look the same: head and tail pointer are equal. There are two approaches to differentiating between full and empty:

    “Waste” a slot in the buffer:
        Full state is head + 1 == tail
        Empty state is head == tail
    Use a bool flag and additional logic to differentiate states::
        Full state is full
        Empty state is (head == tail) && !full

# Linux Driver
## Materiały:
* https://www.apriorit.com/dev-blog/195-simple-driver-for-linux-os#building-kernel
* https://www.beyondlogic.org/usbnutshell/usb2.shtml
* https://crescentro.se/posts/writing-drivers/
* https://stackoverflow.com/questions/22632713/how-to-write-a-simple-linux-device-driver
* https://docs.kernel.org/driver-api/usb/usb.html
* https://docs.kernel.org/driver-api/usb/writing_usb_driver.html
* https://docs.kernel.org/userspace-api/gpio/sysfs.html (sysfs gpio [obsolete don't use outside of interview take home tasks])
* http://www.linux-usb.org/gadget/ (Will this be able to mock a device?)
* https://www.kernel.org/doc/html/v4.19/driver-api/usb/gadget.html
* https://github.com/xairy/raw-gadget
* https://docs.kernel.org/usb/gadget_configfs.html
* https://www.reactivated.net/writing_udev_rules.html
* https://github.com/piersfinlayson/tinyusb-vendor-example
* sudo cat /sys/kernel/debug/usb/devices
* lsusb -s 001:006 -v

## Plan:
1. libUsb na komputerze & tinyUsb na urządzeniu
    b. pierw spróbować obsłużyć np klawiaturę przez libusb??
        1. Znajdź pico przez libusb
        2. otwórz
        3. Prześlij/odbierz dane.
        ----
        1. To samo z gadgetem, może przez ten interfejs configfs (https://docs.kernel.org/usb/gadget_configfs.html)
    a. jak zamockować urządzenie usb do testowania?
    c. co to jest udev, modprobe, ioctl
2. profit (Sterownik usb dla linuxa)
3. własna implementacja usb na urządzeniu???
4. 2 konfiguracje (dla sterownika i klawiatura hid [dla play/pause, mute i jeśli nie połączy się ze sterownikiem])ŚP przetwórstwa rolnego w Polsce, zostały one zainwestowane w 100%. Zdecydowana większość wsparcia ze środków europejskich idzie na pol
(Co to jest tam powyżej? xD)