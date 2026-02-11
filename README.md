# NRF52_Zephyr_BLE_Client
We will implement a BLE client on the nrf52840DK board using Zephyr.

## General description
When I picked up the WB5, I never managed to get to the point where I turned it into a client instead of a server. To be fair, I never even attempted it since it wasn’t event clear if it could be set up as a client or it has the libraries only for server activities.
For the nrf52840, I know for a fact that it works since I have already come across code running on an Adafruit nrf52 ItsyBitsy that was behaving as a client. Being reinforced in my idea that the nrf52 will do client behaviour, I wish to generate such function on the DK board and thus close off this repo sequences by touching upon all the immediate practical applications for BLE. (I won’t be looking into SMP and security though.)

Anyway, how can we do a client? Unlike the server setup which had an entire training to follow on the Nordic website, we don’t have anything like that for the client side! We will have to go into the Zephyr documentation and then try to create a client by shadow boxing the problem until it starts to work…

Or do we?

Well, luckily, no we don’t. Well, not completely.

### RSSI
I have not talked about signals much between our devices since we were using our phones as client which had more than enough umpf in it to connect to anything we were setting up.

That is going to change now so let’s share a word or two on RSSI.

RSSI is the received signal strength indicator and shows us, how strong the signal is between two devices (i.e. how much power the incoming signal carries when reaching the receiver). RSSI is measured in dBm (decibels referenced to milliwatt). Since it is dB, it is exponential, meaning that the difference between 0 dBm and 20 dBm is a 100 fold (1 mW versus 100 mW).

Since the signal strength is distance-related it can be used to estimate the distance between transmitter and receiver. BLE signal strength is rather limited - less than 20 dBm, which gives it less than 100 m range. At the start, we usually have 0 dBm though which is about a meter in range.

Mind, the less the RSSI value, the less the signal strength. We can extract the rssi from the BLE stack.

## Reverse engineering
There is no need to reinvent the wheel if we can just get the specs for it and build one ourselves.

I was already using reverse engineering as a design philosophy when I picked up the WB5 and found the example projects it had for the button push and heart rate examples. There I attempted to strip from the example everything that I did not need while also trying to come to terms with what was actually happening in the code. The results were lacking but they worked and allowed me understand BLE to some extent. Luckily, we have ample amount of BLE client example code provided by Zephyr. We just need to figure out, how they work and then modify them according to what we wish to do.

Talking about WB5, we will need a server to do our client work. I suggest getting hold of the WB5 and using it as the server for our client, albeit any other BLE server device would do, given we know all there is to be had regarding its profile. I am sticking with the WB5 since I know everything about it, thus making the easiest to probe with the nrf52 client.

Mind, the example code are not supposed to be built for the DK board, thus modifications in the devtree and the KConfig might be necessary.

## Previous relevant projects:
We will be using the server code I have written for the WB5 as our server:

STM32_BLE_Custom_Server_Read

We will be carrying over many things from the BLE server implementation for the nrf52 as well:

NRF52_Zephyr_BLE_Server

## To read
We will use the Zephyr documentation extensively.

All Zephyr examples can be found here:
https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/bluetooth

## Particularities
Anyway, let’s look at the example code, shall we?

### Example code: “central”
The simplest client code there to be found called “central”. This example will do a passive scan of the BLE bus and connect to anything that has a greater signal strength than -50 dBm. Once successful, the client simply disconnects from the server and carries on scanning until a new server comes along.

As probably guessed from the description, our activities here will remain on GAP level, thus making this example code an ideal baseline to work from. We will take it, dissect it and then rebuild it to be used as our own client code.

Mind, the code is supposed to be built for the board called the “OpenISA Vega” which is a dual core RISC-V mcu. Luckily, it doesn’t matter here since the Vega also uses the same software library as we are going to do to generate the BLE stack, albeit it runs it in a designated coprocessor.

Anyway, let’s go through each file and check, what needs to be set:

#### Devtree
Normally we don’t need to look at the devtree, unless we are using some specialised hardware to run the BLE stack. Here we don’t have any of that, thus we can port our code to the DK board – with the v9 board definition – and the sample will run fine. This also means that any other examples we might come across that are made for the Vega should work with our existing definitions without a hitch.

#### Defconfig
Usually, we will have to modify this just as we had to when calibrating the server. For this simple client, “CONFIG_BT=y” and “CONFIG_BT_CENTRAL=y” must be added.

#### CMakeList
Nothing to see here.

#### main.c
With the hardware definition dealt with, let’s take a dive into the definitions, functions and macros the sample code is using:

- static struct bt_conn *default_conn: we already saw this. This will be the connection handle. We will be using this handle here this time to reference the BLE connection when we create it.
-  bt_conn_unref(default_conn): dereference the connection handle. Must be done every time we release the server.
-  BT_CONN_CB_DEFINE(conn_callbacks): connection callback states. Will use the callbacks from “connected” and “disconnected” here.
-bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr)): extract connection address from the connection handle into a char array. (Here we extract the address from an existing connection, though connecting to call this function is not necessary. With the “device_found” callback, we will have the address already, for instance.)
- bt_enable(NULL): enables the BLE
- bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found): defines the scanning type and the callback function in case a device is found
- bt_le_scan_stop(): stop the scanning
- bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn): create a connection with the server that was found on address “addr” with the standard scan parameters (connection option, scan interval, scan window) and the standard connection parameters (interval, latency, timeout). The connection parameters should be kept standard in order to avoid missing the communication with the server.
- bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN): disconnects client from server. Should give a reason for the dc.

Ant the callbacks, of course:

- connected: callback from the BLE connection state (same as for server). Disconnects from the device right after connecting. Takes the connection handle and the error code as input.
- disconnected: callback from the BLE connection state. Un-references the BLE handle and restarts scanning. Takes the connection handle and the error code as input.
- device found:  “bt_le_scan_cb_t” type callback for “bt_le_scan_start”. Stops scanning in case a device with less than -50 RSSI is present.  Does NOT take the connection handle as parameter, thus it has to be fed as a static variable. Extracts the type of the server (public or not)and the address of the server (“bt_addr_le_t” struct), the advertising type (scannable, connectable, directed, etc.), the signal strength rssi and the advertiser data (into a buffer). The callback then connects to the server. 

#### bt_conn confusion
“bt_conn” does not seem to be a Zephyr struct, at least I have not managed to find it in the documentation. It seems more like an arbitrary handle – like a name – that our code can use to reference the connection for various reasons. I don’t know, why this is the case when “bt_conn” is an integral part for handling the connection in Zephyr.

It is currently unclear to me, what incrementing the “reference count” of this “bt_conn” handle does to us, apart from assigning it a number. I assume it helps to classify connections if we have multiple of them ongoing at the same time as well as track the progression of connections/disconnects. Removing the references/unreferences inhibits the code from restarting the connection after a dc, so it must be important.

#### Build and flash the sample code
Let’s build the code and flash it to the DK board with the v9 board configuration.

The nrf52 will immediately start scanning the BLE bus for devices and then spew back every and each one it finds. It will only attempt connecting to devices that are close by – signal strength greater than -50 dBm – which ideally should only include our WB5 (if not, move further away the boards from other BLE servers). The code will connect and disconnect from the WB5, then go back to scanning.

### BLE_Client_GAP_test
Let’s take the “central” sample code and modify it to our needs.

#### New board definition
Before anything, we will have to create a new board definition file from the existing v9. Here we merely need to add  “CONFIG_BT_CENTRAL=y”  and include the “CONFIG_LOG=y” in case we wish to use the logger module (we do). The rest remain the same.
Technically, with this move, we are eliminating the need for the “proj.conf” completely. 

#### Strip
What I suggests right off the bat is to remove the “device found” print and then add a “k_msleep” right after the rssi test. This makes our code print out information only when a connection was successful and wait some time after each connection.

We can also safely remove the uuid.h and gatt.h libraries since we aren’t using. We can also remove the “hci.h” if we remove the error extraction function (“bt_hci_err_to_str”). We can also remove the various connections type tests and remain only with the “rssi” test (with rssi < -50 ignored, that should mean only the WB5 will be connected to, though a particularly BLE noisy environment may demand restricting the rssi further). We will leave the “bt_addr_le_to_str” function from the “addr.h” so as to have at least some form of feedback for a successful connection.

With that, we will have a code that is as bare-bones as it could be. It won’t do much else than what the example should be doing, using only the functions from “Bluetooth.h” library – bt_enable, bt_le_scan_start, bt_le_scan_stop – and the “conn.h” library - bt_conn_ref, bt_conn_unref, bt_conn_le_create, bt_conn_disconnect.

The connection “Bluetooth.h” includes the “addr.h” library (address management functions), the “gap.h” (connection setup parameters) and the “hci_types.h” (connection macros and structs). The “conn.h” includes the “Bluetooth.h” library. Mind, we only need to include “conn.h” to make the bare-bones version work, but I am re-including every library in order to offer more clarity on the libraries we are using anyway.

#### Dress back up
With the code stripped, we will now modify it to print out not just the address of the connected device, but additional advertising data, such as the name of the device. As a matter of fact, we will restrict our scan to find a device with the name “WB5MM” only.
First and foremost, we will replace all “printk” elements with the logger. It is to avoid printk crashing the BLE stack in the follow-up projects, see previous repos for why that is an issue.

We can then add the connection types investigation (elements are in “gap.h”) to limit, which devices we intend to interact with. We should add a check that we would only connect to devices that are connectable (though we could search for beacons only too, should we wish to do that). Mind, the “gap.h” struct is used to interrogate the connection type since that is what our device found callback (“bt_le_scan_cb_t”) is using as the type descriptor (“bt_gap_adv_type” enum – NOT a struct!). (Just a note here, in practicality, we were defining the type as “BT_LE_ADV_OPT_NONE” using the “bluetooth.h” library in the previous repo. Again, there is redundancy here and we should pay attention, what we are extracting with our callbacks and what “definition” that information aligns to.)
Afterwards, we are (re-)adding error messaging and thus make use of the “hci.h” library. This library is what handles the host controller, thus using it will allow us to interact with the controller directly, either sending commands to it or just extracting information, such as error messages. We can add “bt_hci_err_to_str” to write the hci error into a string when the stack encounters a problem.

When scanning, the example is using passive scanning which means that our client does not ask for additional information from servers. In other words, our client does not send a “scan request” when using passive scanning. For our case, the WB5 will give the same amount of replies for either PASSIVE or ACTIVE scanning with the reply including only the name of the server – here the WB5MM string (which will be 13 bytes long, by the way).

We extract the name of the server from the data section and format it as a char array to compare it to a static defined device char array (the device name). Now, we can remove the rssi and device type testing from the code if we wish to since we will only connect to the WB5MM…

#### Summary
Code section: we have a client code that will search for a device named “WB5MM” then connect to it if it is close enough. After connection, the client disconnects immediately. The connection is using standard parameters and does not use GATT. Terminal printing is done using the thread safe logger module.

Board type: custom board v10, WB5MM programmed up using “STM32_BLE_Custom_Server_Read” repo on github

### Example code: “central_gatt_write”
We will look at the example code “central_gatt_write” which scans for a device, connects to it and then changes the value of a characteristic – effectively a “write without response” done by the client using GATT.
As such, all the things that we let the client do automatically in the server code – parameter update, mtu change and so on – will have to be done by us now.

Unlike in the previous example, it will not disconnect from the server in code, only in case of a bus failure/connection timeout. If a dc occurs, the client device simply unreferences the connection handle and restarts scanning.

Also, our “main” function will not be the “main” superloop but a function called “central_gatt_write” which will behave exactly the same as a superloop with a “setup” part, followed by an infinite “while” loop that will be executed non-stop.
Lastly, we won’t be using SMP pairing.

#### Devtree
There are no devtree files attached to the example.

#### Defconfig
We see that we have the “CONFIG_BT_GATT_CLIENT=y” added with various buffer definitions. The MTU definition, the ACL_RX and ACL_TX buffer sizes are the same as what we had when we were setting up our server since they all relate to the GATT topology which should match between the two devices, no surprises there. We will have “CONFIG_BT_BUF_CMD_TX_SIZE=255” and “CONFIG_BT_BUF_EVT_DISCARDABLE_SIZE=255” added though, plus the “CONFIG_BT_SMP=y”, this latter being the security manager that allows pairing using BLE. (We won’t be using pairing.)

#### CMakeList
Nothing to see here.

#### main.c/central_ gatt _write.c/ gatt _write_common.c
This example has a more complex project structure I assume because to fit the original coder’s standardisation purposes. At any rate, we will have the “main.c”, “central_ gatt _write.c” and “gatt _write_common.c” source files where we have only one function in the “main.c” which calls the “central_ gatt_write” function.

“central_GATT_write” if our main function, which will enable the BLE, define the GATT callbacks, “write_cmd“ write command and references/unreferences the connection handle. The GATT callback will only be for the mtu update. The GATT write function takes a counter in as input to indicate the number of GATT writes. Mind, the while loop in the “central_gatt_write” will execute forever and we will use a connection check to tell the loop to write to the server or just tick over without doing anything. The thread running the function will yield after writing or go to sleep for one second in case there is no connection.

Within the source code holding this function, we will have the “start_scan” with its “device_found” callback function, both identical to the previous example (except for doing an ACTIVE scan, not a PASSIVE one).

The rest of the functions will be in the “gatt _write_common” source file. Here we will have the connection callbacks (connection, not the GATT) defined - connected, disconnected, parameter changed and parameter requested – the mtu exchange function running “bt_ gatt _exchange_mtu” and its callback (remember from the previous repo, the mtu change asks for a struct with a callback function as input. The exchange is exactly the same as on the server side).

Regarding connection callbacks, the param request and the param update are just printouts. The connect and the disconnect also do printouts, plus references and un-references the connection handle. Apart from that, the connect callback includes the mtu update and the disconnect a scan restart.

We will also have the “write_cmd” – calling “bt_gatt_write_without_response_cb” with its own callback function – here in the “common” source file and this function will be the really new part in this project. The function is called by the “while forever” loop in the “central_gatt_write” function (i.e. the example’s superloop) in case there is a connection with the server.

Let’s look at “write_cmd” a bit closer.

Firstly, we do an incoming data package size check and, in case more data is to be transferred than what is the maximum capacity of the bus, we cut the package up. Mind, this only works if we enable the fragmentation, which we won’t be doing here. Then comes the GATT write function with its callback:

- bt_gatt_write_without_response_cb: the write without response function for GATT client to modify the value section of an ATT attribute. Takes in the connection handle, the attribute handle (the first uint16_t of the UUID), the data pointer, the data len, a signature boolean, the callback function and the data to pass to the callback (here, the data package length).
- write_cmd_cb: the callback of the write function of “bt_gatt_complete_funct_t” type. Takes in the connection handle and a user data void pointer as input. The user data will be defined by the last input variable of the response function (see above). Mind, the callback will be automatically executed within a work queue.

Since we are referencing everything through handles, technically speaking, we could write to service attributes as well using a “write GATT” function, though normally services are read only in a server thus only characteristics’ value sections would be accessible by the client.

Looking at the GATT write callback, we are checking the time passed by reading the clock of the kernel – “k_cycle_get_32” - and transform the time to nanoseconds – “k_cyc_to_ns_floor64”. If we see that the time passed between the two write callbacks is greater than 1 second, then we had a dc and all externally defined (static) connection metrics are reset. If the time difference is less than 1 second, we update the amount of data sent metric as well as the writing rate. Mind, if we do not need externally defined static variables to track the metrics of our connection, we won’t need to have a GATT write with a callback.

#### Build and flash the sample code
We will build the example to the v10 board. The client then connects to the WB5MM (if it is close enough) and starts sending it some data packages. Mind, we will only see this on the terminal of the nrf52 since the example GATT write – out of the box - is not targeting the write characteristic within the WB5MM, nor does it care if the server can do anything with the incoming information or not. After all, we are writing without a response…

Heck, it doesn’t even have anything in the data buffer that it is publishing!

Anyway, if we want to direct the client to match what is on the server side, we will have to modify the example to our taste.

### BLE_Client_GATT_test
We will carry over the project “BLE_Client_GAP_test” and use it as the baseline for our current project.

#### WB5MM recap
What we want to do is connect to the WB5MM and interact with the three services in it.

Just to recap, we have a:

- WRITE service with UUID 0x0001 and a characteristic of UUID 0x0002 where updating the characteristic will print the updated value ion the OLED screen of the WB5
- NOTIFY service of UUID 0x0003 with a characteristic UUID of 0x0004 where enabling the notification will start a counter. The counter is then published on the OLED screen as well as published within the notification.
- READ service of UUID 0x0005 and characteristic UUID of 0x0006 where reading out will read out an attached BMP280 sensor’s ID and send the value to the client.

If the UUID values are not these, modify your WB5MM code to match them.

#### New board definition
Before anything, we will have to create a new board definition file from the existing v9. Here we merely need to add  “CONFIG_BT_CENTRAL=y”  and include the “CONFIG_LOG=y” in case we wish to use the logger module (we do). The rest remain the same.
Technically, with this move, we are eliminating the need for the “proj.conf” completely. 

#### Add more layers
We won’t strip down the example this time, instead we will carry over the project “BLE_Client_GAP_test” and use it as the baseline for our current project. This should connect us to the WB5MM right off the bat with most of the callbacks already rather well defined.
The “start_scan” and the “device_found” callback are identical, except we will be doing an ACTIVE scan and add a connection handle de-reference to the callback. We can also remove the connection delay we have in the “device_found” callback since we won’t be cycling connects and disconnects anymore.

Regarding connection callbacks, we will not carry over the parameter request and parameter update connection callbacks. This disconnect callback is practically the same as what we had already just that we are using the “bt_conn_get_info” function to extract, well, the connection information and do a printout in case it fails. For the connected callback, we need to remove the automatic disconnect and add a handle reference, followed by the mtu exchange function.

As we have seen in the previous repo, the mtu exchange is a function + callback action. Here the callback will be just a printout. We define a GATT callback on the client side for the mtu change to ensure that the parameter update has happened.
In the main loop, we have to add an infinite loop with the connection test. If there is connection, we send the write command and dereference the connection.

We will carry over the write command from the example file. We can simplify it by removing the callback version completely of the write and go with “bt_gatt_write_without_response” since we won’t need it (for most actions). I recommend keeping it though and use it to disconnect from the WB5MM.

In place of the connection delay in “device_found”, I have added a delay within the super loop itself.

#### When we flash….
When we flash the code the nrf52, we will not have the expected outcome on the WB5MM, that is, we won’t see the sent data on the OLED screen, even though we won’t have any errors on the nrf52 side. We will have the connection established and the mtu set though.
Exploring the WB5MM – that is, running the code on it while in debug mode and check, what is going on – will indicate that indeed, we have connection between the WB5MM and the nrf52, but every time when we send the write command over, the WB5MM will skip reacting to it in its state machine (see “custom_stm.c”). The problem is that our custom server will react only to a handful of event codes and running the nrf52 will not generate one of those: we get the proper event code and expected execution when activating the “write” service on the WB5MM using our phones though. The successful transfer event code is “3073” byt the way, while using the nrf52 with handle 0x0002, we get an event code “3075” instead.

The chief problem is that there is no documentation to my knowledge to explain, what each event code means in the ST BLE stack. I did get the proper event code when writing to handle 0x0004 a single byte (0x0004 is the notification characteristic).

By doing a bit of trial and error, it became clear that sending a data package greater in size than the ATT value len would result in the error code: if I have sent more than 2 bytes to 0x0004, the event code became “3075” there as well. According to the WB5MM, the 0x0004 ATT has a length of 2 bytes. This indicates to me that the WB5MM – as it is running out code from before – is very sensitive to the data transfer length. If we have event code “3075”, the stack will practically collapse giving us back a lot of corrupted data (like the handle uuid will be completely fake). Anyway, the code above is likely an error event that triggers if the client’s GATT write command does not align to the server’s GATT service.

There is another strange thing with the WB5MM where it uses different handles/UUIDs internally than what we can see/give it by using CubeMx. To be more precise, there is a 0x0c offset. To even make things worse, it doesn’t handle ATT values by their individual UUID but by how much they are offset from the service. In other words, we should not write to handle “0x0002” with our client, but something like “0x000c+0x0002+0x0001”…

#### Implementing a workaround
As it turns out, the “write” we have set up in the WB5MM is a “write WITH response”, thus it is using the normal GATT write command and not the one without response. Using the “non response” version will simply not work. Anyway, this write function uses a “bt_gatt_write_params” struct to drive it thus it should be then filled up properly. The callback function here is obligatory and is of “bt_gatt_write_func_t” type.

At the same time, the notification does use the “write without response” command to enable itself (change the notification characteristic descriptor ATT value), thus we will have to use two different type of write commands to make use of the two different services we are running in the WB5MM.

Also, we will have to use our write command to UUID handle 0x0012 to enable the notification on UUID 0x0004 (0xc for the offset + 0x4 for the UUID + 0x2 for the characteristic’s descriptor) and UUID 0x000e for our write ATT on UUID 0x0002 (0xc for the offset + 0x1 for the UUID + 0x1 for the characteristic’s value). These handle values are offset within the WB5MM code for unknown reasons.

Of note, once the notification is enabled, it won’t stop unless we reset the WB5MM or send it the turn off command.

#### Readout
Making the readout work is pretty self-explanatory. The only thing there is to be aware of is that we can have the readout in three different ways where we can read a single handle, multiple handles or a range of uuid values. The “handle_count” parameter in the “bt_gatt_read_params” struct will be the one selecting, which one of the three we will have.

For reading out the read characteristic, we will have to use the handle 0x0015 for similar reasons as discussed above. I have extracted the handle for reading out with the phone and running the debugger on the WB5MM.

The data is then read out by the readout’s “bt_gatt_read_func_t” type callback function, 

#### Summary
Code section: we have a client code that will search for a device named “WB5MM” then connect to it if it is close enough. After connection, the client will either write to the WB5MM OLED screen a char array or turn on/off a counter or read out a temperature value (in case there is a sensor attached to the WB5MM, if not, the value “0xd5”). The selection between actions is done by choosing the right “#ifdef” section within the code.

Board type: custom board v11, WB5MM programmed up using “STM32_BLE_Custom_Server_Read” repo on github

IMPROTANT! The GATT communication in this project is precisely tailored to what the WB5MM asks for. The WB5MM is NOT a properly programmed BLE server though as I have indicated before, thus most modifications in code regarding uuid values (i.e. the offsets) should be discarded in case the code is used for a different type of server.

### Board versions
The new board versions are the following:

-	V10: added BLE Client GAP
-	V11: added BLE Client GATT
As before when we did the server, here we are exclusively working in the defconfig file and leave the rest of the devtree untouched compared to board version v6. The board could be simplified heavily though since we aren’t using any of the peripherals in the client.

## Conclusion
And with that, we have concluded out Zephyr studies with the successful implementation of a fully functional BLE client using an nrf52840DK board.

Learning Zephyr had been great fun and I can full heartedly recommend it in case someone is looking for a challenge. Once getting a grip of it, it is surprisingly easy to use and is significantly more stable than other HAL and RTOS solutions I have come across to this day.
Anyway, I hope this project will help someone out in the future. I certainly will use it to implement my future Zephyr projects.
