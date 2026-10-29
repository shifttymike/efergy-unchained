XC8 ?= $(CURDIR)/.toolchain/xc8-v4.00/bin/xc8-cc
DFP ?= /Applications/microchip/mplabx/v6.20/packs/Microchip/PIC18F-J_DFP/1.7.159/xc8
CC_HOST ?= clang
CPPFLAGS = -Ibuild -Iinclude -Ivendor/microchip/include $(EXTRA_CPPFLAGS)
CFLAGS = -mcpu=18F67J60 -mdfp=$(DFP) -O2 -std=c99 -mwarn=-3
STACK = ARP DHCP DNS ETH97J60 Helpers ICMP IP StackTsk TCP Tick UDP Delay
SOURCES = $(filter-out src/radio_sampler.c,$(wildcard src/*.c)) $(wildcard src/*.S) $(addprefix vendor/microchip/src/,$(addsuffix .c,$(STACK)))

.PHONY: all clean check assets
all: build/efergy-factory.hex
build/efergy.hex: build/web_assets_data.h $(SOURCES) $(wildcard include/*.h)
	@mkdir -p build
	$(XC8) $(CFLAGS) -mcodeoffset=0x400 -mrom=0x400-0x1cbff,0x1fff8-0x1ffff $(CPPFLAGS) $(SOURCES) -o $@
clean:
	rm -rf build
assets: build/web_assets_data.h
build/web_assets_data.h: assets/index.html tools/build_assets.py
	python3 tools/build_assets.py
check: build/web_assets_data.h build/efergy-factory.hex
	@mkdir -p build
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_core.c src/e2_decoder.c src/radio_sampler.c -o build/test_core
	./build/test_core
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_protocol.c src/config.c src/mqtt_codec.c -o build/test_protocol
	./build/test_protocol
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_http.c src/http_parser.c src/config.c -o build/test_http
	./build/test_http
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -DCONFIG_WRITES_ENABLED=1 -DEFERGY_HOST_TEST -Iinclude tests/test_store.c src/config_store.c src/config.c src/app_workspace.c -o build/test_store
	./build/test_store
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/stubs -Iinclude tests/test_mqtt_client.c src/mqtt_client.c src/mqtt_codec.c src/config.c src/app_workspace.c -o build/test_mqtt_client
	./build/test_mqtt_client
	python3 -c 'import json,pathlib; [json.loads(p.read_text()) for p in pathlib.Path("build").glob("*-test.json")]; print("MQTT discovery/state JSON parsed")'
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/stubs -Iinclude tests/test_network.c src/network.c src/config.c -o build/test_network
	./build/test_network
	python3 tests/test_ipv4.py
	python3 tests/test_dhcp_hostname.py
	python3 tests/test_radio_timing.py
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/stubs -Iinclude tests/test_http_server.c src/http_server.c src/http_parser.c src/config.c src/app_workspace.c -o build/test_http_server
	./build/test_http_server 0
	./build/test_http_server 1
	./build/test_http_server 2
	./build/test_http_server 3
	./build/test_http_server 4
	./build/test_http_server 5
	./build/test_http_server 6

	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Ibuild -Itests/stubs -Iinclude tests/test_web_assets.c src/web_assets.c -o build/test_web_assets
	./build/test_web_assets
	python3 tests/test_assets.py
	node tests/test_ui_polling.js
	node tests/test_ui_history.js
	$(CC_HOST) -O2 -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude -Ibuild tests/test_ota_boot.c bootloader/boot_flow.c src/ota_format.c src/ota_store.c -o build/test_ota_boot
	./build/test_ota_boot
	$(CC_HOST) -O2 -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/stubs -Iinclude tests/test_ota_update.c src/ota_update.c src/ota_format.c src/ota_store.c -o build/test_ota_update
	./build/test_ota_update
	python3 tests/test_ota_package.py

# The factory build owns both permanent vector/loader regions. Never upload it.
build/factory_image.h: build/efergy.hex tools/package_firmware.py
	python3 tools/package_firmware.py build/efergy.hex
build/bootloader.hex: bootloader/main.c bootloader/boot_flow.c bootloader/vectors.S src/ota_format.c src/ota_store.c include/ota_format.h include/ota_store.h build/factory_image.h
	$(XC8) $(CFLAGS) -mcodeoffset=0x1cc00 -mrom=0-0x3ff,0x1cc00-0x1ffff -Iinclude -Ibuild bootloader/main.c bootloader/boot_flow.c bootloader/vectors.S src/ota_format.c src/ota_store.c -o $@
build/efergy-factory.hex: build/bootloader.hex build/efergy.hex tools/package_firmware.py
	python3 tools/package_firmware.py build/efergy.hex --bootloader build/bootloader.hex

# Experimental one-time stock-loader carrier. Never serve the factory HEX to OEM.
.PHONY: oem-transition
oem-transition: build/efergy-factory.hex
	python3 tools/package_oem_transition.py --factory build/efergy-factory.hex --package build/efergy-update.efu --out build/transition
	$(XC8) $(CFLAGS) -mcodeoffset=0x400 -mrom=0x400-0x5fff -Iinclude -Ibuild/transition bootstrap/main.c src/ota_format.c -o build/transition/bootstrap.hex
	python3 tools/package_oem_transition.py --bootstrap build/transition/bootstrap.hex --out build/transition
	python3 tests/test_oem_transition.py

.PHONY: oem-restore oem-restore-check
oem-restore: assets
	@test -n "$(OEM_HEX)" || (echo 'Set OEM_HEX to the full stock PIC HEX'; exit 1)
	python3 tools/build_oem_restore.py "$(OEM_HEX)" --xc8 "$(XC8)" --dfp "$(DFP)"
oem-restore-check:
	$(CC_HOST) -O2 -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -DOEM_RESTORE_BUILD=1 -DOEM_RESTORE_HOST_TEST=1 -Itests/stubs -Iinclude -Ibuild/oem-restore tests/test_oem_restore.c src/oem_restore.c src/ota_format.c -o build/test_oem_restore
	./build/test_oem_restore
	$(CC_HOST) -O2 -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -DOEM_RESTORE_HOST_TEST=1 -Iinclude tests/test_oem_hex.c src/oem_hex.c -o build/test_oem_hex
	./build/test_oem_hex build/oem-restore/oem-pic.bin build/oem-restore/generated-test.hex
	cmp build/oem-restore/generated-test.hex build/oem-restore/oem-application.hex
	$(CC_HOST) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -DOEM_RESTORE_BUILD=1 -Iinclude tests/test_http.c src/http_parser.c src/config.c -o build/test_oem_http
	./build/test_oem_http
