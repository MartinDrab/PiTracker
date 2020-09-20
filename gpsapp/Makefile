TARGET=gpsapp
CFLAGS ?= -O3 -pipe
CFLAGS += -Wall --std=gnu99 -DNDEBUG -Wno-unused-function
OBJDIR=./obj

OBJ=\
	$(OBJDIR)/gps.o	\
	$(OBJDIR)/logging.o	\
	$(OBJDIR)/settings.o	\
	$(OBJDIR)/serial.o	\
	$(OBJDIR)/commands.o	\
	$(OBJDIR)/field-array.o	\
	$(OBJDIR)/line-buffer.o	\
	$(OBJDIR)/accounts.o	\
	$(OBJDIR)/cmdline.o	\

.PHONY: all
all: $(TARGET)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	@echo Compiling $<...
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJ)
	@echo Linking $@...
	@$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) $(LDLIBS) -o $@


.PHONY: clean
clean:
	@echo Cleaning up...
	@$(RM) $(OBJ) $(TARGET)
