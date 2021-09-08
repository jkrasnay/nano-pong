FQBN = arduino:avr:nano
PROJECT = nano-pong
BUILD_PATH = $(PWD)/target

# USB hub
#PORT = /dev/cu.wchusbserial144140

# Direct connect
PORT = /dev/cu.wchusbserial1420


.PHONY: all compile upload clean

all: upload

compile:
	arduino-cli compile --build-path $(BUILD_PATH) --fqbn $(FQBN) $(PROJECT)

upload: compile
	arduino-cli upload --input-dir $(BUILD_PATH) --port $(PORT) --fqbn $(FQBN) $(PROJECT)

monitor:
	# You can't upload while monitoring
	# Hit Ctrl-A, K to kill screen
	# It sure would be nice if arduino-cli had a built-in monitor mode
	# with commands suspend and re-connect the monitor that you could
	# call before and after `upload`
	screen $(PORT) 9600

clean:
	@rm -rf $(BUILD_PATH)
