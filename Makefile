CC = gcc
PROJECT_NAME = cura8-reader

INC_DIRS += \
	Inc \
	
SRC_FILES += \
	bluetooth-shell.c \
	main.c \
	gatt.c \
	util.c \
	mainloop.c \
	client.c \
	object.c \
	polkit.c \
	watch.c \
	mainloop-glib.c \
	io-glib.c \
	ble_data_obj.c \
	ble_data_sub.c \
	csv_logging.c \
	data_logging_handling.c
	
OUT_DIR = Output

dir_guard = @mkdir -p $(OUT_DIR)

CFLAGS += $(addprefix -I,$(INC_DIRS))

LIBS += `pkg-config --libs --cflags glib-2.0 dbus-glib-1` -pthread

info:=$(shell tput setaf 4)
result:=$(shell tput setaf 5)
reset:=$(shell tput sgr0)

$(OUT_DIR)/%.o: %.c
	$(dir_guard)
	$(info $(info)[$(CC)]:$(reset)$< -> $(OUT_DIR)/$(notdir $@))
	@$(CC) $(LIBS) $(CFLAGS) -c $< -o $(OUT_DIR)/$(notdir $@)

$(OUT_DIR)/$(PROJECT_NAME): $(info $(info)[$(CC) = $(shell gcc -dumpmachine)]$(reset)) $(patsubst %, $(OUT_DIR)/%, $(SRC_FILES:.c=.o))
	$(dir_guard)
	$(info $(info)[$(CC)]:$(reset)$(LIBS) $(notdir $^) -> $(OUT_DIR)/$(PROJECT_NAME)) 
	@$(CC) $(LIBS) $(CFLAGS) -o $@ $(foreach file,$(notdir $^),$(wildcard $(OUT_DIR)/$(file)))
	$(info $(result)Build done!)

.PHONY: clean run

clean: 
	@rm -f $(OUT_DIR)/*
	$(info $(result)Cleaned!) 

run: $(OUT_DIR)/$(PROJECT_NAME)
	$(info $(result)$(PROJECT_NAME) started!$(reset))
	@./Output/\$(PROJECT_NAME)