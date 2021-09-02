FQBN = arduino:avr:nano
PORT = /dev/cu.wchusbserial144140
PROJECT = nano-pong
BUILD_PATH = $(PWD)/target

.PHONY: all compile upload clean

all: upload

compile:
	arduino-cli compile --build-path $(BUILD_PATH) --fqbn $(FQBN) $(PROJECT)

upload: compile
	arduino-cli upload --input-dir $(BUILD_PATH) --port $(PORT) --fqbn $(FQBN) $(PROJECT)

clean:
	@rm -rf $(BUILD_PATH)
