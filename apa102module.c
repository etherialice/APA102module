/* (c) Spencer Tinnin 2018 based off of code written by Martin Erzberger*/

#include "Python.h"

typedef uint8_t byte;

static const byte BYTES_PER_LED = 4;
static const byte MAX_BRIGHTNESS = 31; // Safeguard: Max. brightness that can be selected. 
static const byte LED_START = 0b11100000; // Three "1" bits, followed by 5 brightness bits
static const byte LED_BRIGHT_MASK = 0b0011111;
static const byte RED = 0, GRN = 1, BLU = 2;

static PyObject *ErrorObject;

PyDoc_STRVAR(apa102_module_doc,
	"This module defines an object type that allows the user to control a string of APA102 pixels (also known as the Adafruit DotStar)\n"
	"to create one use the following syntax:\n"
	"APA102(num_led, spi_write, [global_brightness=31], [order=\"RGB\"])\n"
		"\tnum_led -- how many leds you want to drive\n"
		"\twrite_callback -- the function called to send data to the leds if set to None, show() will do nothing\n"
			"\t\tthis was designed with spidev.write() in mind, but any function that takes in a list of numbers will work"
		"\tglobal_brightness (optional) -- a number from 0 to 31 (inclusive) determining how bright the pixels show \n\t\tNote: a brightness of 0 will mean the pixels never turn on)\n"
		"\torder (optional) -- the order that your strip takes the red, green, and blue values as a string\n"
	"Public Functions:\n"
		"\tset_pixel\n"
		"\tset_pixel_rgb\n"
		"\tset_range\n"
		"\tset_range_rgb\n"
		"\tset_range_rgb\n"
		"\tset_all\n"
		"\tset_all_rgb\n"
		"\tshow\n"
		"\tclear_strip\n"
		"\trotate\n"
		"\tget_pixel_color_str\n"
		"\tget_pixel_color_rgb\n"
		"\tget_pixel_color\n"
		"\tcombine_color\n"
		"\twheel\n"
		"\tdump_array"
	"Variables (all are read only):\n"
		"\tnum_led\n"
		"\tglobal_brightness\n"
		"\torder\n"
		"\twrite_callback\n"
		"\tMAX_BRIGHTNESS\n");

typedef struct {
	PyObject_HEAD
	PyObject *spi_write;
	byte *leds;
	byte rgb[3];
	byte brightness;
	Py_ssize_t num_led_array;
	int num_led;
} apa102Object;

static PyTypeObject apa102_Type;

#define apa102Object_Check(v)      (Py_TYPE(v) == &apa102_Type)


static int apa102_init (apa102Object *self, PyObject *args, PyObject *keywds){
	
	char *order = NULL;
	self->brightness = MAX_BRIGHTNESS;
	
	static char *kwlist[] = {"num_led", "write_callback", "global_brightness", "order", NULL};
	static const char *types = "iO|bz:__init__";
	
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &(self->num_led), &self->spi_write, &(self->brightness), &order);
	
	
	if (self->spi_write == Py_None)
		self->spi_write = NULL;
	else if (!PyCallable_Check(self->spi_write)) {
			PyErr_SetString(PyExc_TypeError, "parameter spi_write must be callable or None");
			return -1;
	}
	else
		Py_INCREF(self->spi_write);
	
	if (self->brightness > MAX_BRIGHTNESS)
		self->brightness = MAX_BRIGHTNESS;
	
	int use_default_rgb = (order == NULL);
	if (!use_default_rgb) {
		byte r_count = 0, b_count = 0, g_count = 0;
		for (int i = 0; i < 3; i++) {
			char loopC = *(order+i);
			if (loopC == 'r' || loopC == 'R') {
				self->rgb[RED] = 3-i;
				r_count++;
			}
			else if (loopC == 'g' || loopC == 'G') {
				self->rgb[GRN] = 3-i;
				g_count++;
			}
			else if (loopC == 'b' || loopC == 'B') {
				self->rgb[BLU] = 3-i;
				b_count++;
			}
			else if (loopC == '\0')
				break;
		}
		//if r, g, and b don't appear exactly one time each, use the default order
		use_default_rgb = !((r_count == 1) && (g_count == 1) && (b_count == 1));
	}
	
	if (use_default_rgb) {
		self->rgb[0] = 3;
		self->rgb[1] = 2;
		self->rgb[2] = 1;
	}
	
	self->num_led_array = self->num_led * BYTES_PER_LED;
	self->leds = (byte*) PyMem_Calloc(self->num_led_array, sizeof(byte));
	for (int i = 0; (i+3) < self->num_led_array; i+= 4) 
	{
		*(self->leds+i) = LED_START;
	}
	
	//create the objects:
	
	return 0;
}
/* apa102 methods */

static void apa102_dealloc(apa102Object *self)
{
	PyMem_Free((void*)self->leds);
	if (self->spi_write != NULL)
		Py_XDECREF(self->spi_write);
	Py_TYPE(self)->tp_free((PyObject*)self);
}


PyDoc_STRVAR(apa102_set_pixel_doc,
	"set_pixel(led_num, red, green, blue, [brightness=31])\n\n"
	"sets the pixel at led_num to show the given red, green, and blue values (from 0 to 255 inclusive)\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_pixel(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"led_num", "red", "green", "blue", "brightness", NULL};
	static const char *types = "ibbb|b:set_pixel";	
	int led_num;
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &led_num, &r, &g, &b, &led_brightness);
	
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	//if led_num is out of range, do nothing
	if (led_num < self->num_led && led_num >= 0) {
		byte *start_ptr = self->leds + (led_num * BYTES_PER_LED);
		byte bright = (led_brightness * self->brightness) / 31;
		*(start_ptr) = (bright & LED_BRIGHT_MASK) | LED_START;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	Py_RETURN_NONE;
}
PyDoc_STRVAR(apa102_set_pixel_rgb_doc,
	"set_pixel(led_num, rgb_color, [brightness=31])\n\n"
	"sets the pixel at led_num to show the given rgb value in the format 0xRRGGBB\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_pixel_rgb(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"led_num", "rgb_color", "brightness", NULL};
	static const char *types = "ii|b:set_pixel_rgb";	
	int led_num, rgb;
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &led_num, &rgb, &led_brightness);
	
	
	r = (rgb >> 16) & 255;
	g = (rgb >> 8) & 255;
	b = rgb & 255;
	
	
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	//if led_num is out of range, do nothing
	if (led_num < self->num_led && led_num >= 0) {
		byte *start_ptr = self->leds + (led_num * BYTES_PER_LED);
		byte bright = (led_brightness * self->brightness) / 31;
		*(start_ptr) = (bright & LED_BRIGHT_MASK) | LED_START;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(apa102_set_range_doc,
	"set_range(start, end, red, green, blue, [brightness=31])\n\n"
	"sets the pixels from start (inclusive) to end (exclusive) to show the given red, green, and blue values (from 0 to 255 inclusive)\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_range(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"start", "end", "red", "green", "blue", "brightness", NULL};
	static const char *types = "iibbb|b:set_range";	
	int start, end;
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &start, &end, &r, &g, &b, &led_brightness);
	
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	//if start or end is out of range or start is before end. do nothing
	if (start < 0 || end >= self->num_led || start > end)
		Py_RETURN_NONE;
	
	byte bright_byte = (((led_brightness * self->brightness) / 31) & LED_BRIGHT_MASK) | LED_START;
	byte *start_ptr;
	for (int i = start; i < end; i++) {
		start_ptr = self->leds + (i * BYTES_PER_LED);
		*(start_ptr) = bright_byte;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	
	Py_RETURN_NONE;
}
PyDoc_STRVAR(apa102_set_range_rgb_doc,
	"set_range(start, end, rgb, [brightness=31])\n\n"
	"sets the pixels from start (inclusive) to end (exclusive) to show the given color (from 0 to 255 inclusive)\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_range_rgb(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"start", "end", "rgb", "brightness", NULL};
	static const char *types = "iii|b:set_range_rgb";	
	int start, end, rgb;
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &start, &end, &rgb, &led_brightness);
	
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	//if start or end is out of range or start is before end. do nothing
	if (start < 0 || end >= self->num_led || start > end)
		Py_RETURN_NONE;
	
	r = (rgb >> 16) & 255;
	g = (rgb >> 8) & 255;
	b = rgb & 255;
	
	byte bright_byte = (((led_brightness * self->brightness) / 31) & LED_BRIGHT_MASK) | LED_START;
	byte *start_ptr;
	for (int i = start; i < end; i++) {
		start_ptr = self->leds + (i * BYTES_PER_LED);
		*(start_ptr) = bright_byte;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	
	Py_RETURN_NONE;
}

PyDoc_STRVAR(apa102_set_all_doc,
	"set_all(red, green, blue, [brightness=31])\n\n"
	"sets all pixels to show the given red, green, and blue values (from 0 to 255 inclusive)\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_all(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"red", "green", "blue", "brightness", NULL};
	static const char *types = "bbb|b:set_all";	
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &r, &g, &b, &led_brightness);
	
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	
	byte bright_byte = (((led_brightness * self->brightness) / 31) & LED_BRIGHT_MASK) | LED_START;
	byte *start_ptr;
	for (int i = 0; i < self->num_led; i++) {
		start_ptr = self->leds + (i * BYTES_PER_LED);
		*(start_ptr) = bright_byte;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	
	Py_RETURN_NONE;
}
PyDoc_STRVAR(apa102_set_all_rgb_doc,
	"set_all(rgb, [brightness=31])\n\n"
	"sets all pixels to show the given color\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_set_all_rgb(apa102Object *self, PyObject *args, PyObject *keywds)
{
	
	static char *kwlist[] = {"rgb", "brightness", NULL};
	static const char *types = "i|b:set_all_rgb";	
	int rgb;
	unsigned char r, g, b, led_brightness = MAX_BRIGHTNESS;
	PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &rgb, &led_brightness);
	
	r = (rgb >> 16) & 255;
	g = (rgb >> 8) & 255;
	b = rgb & 255;
		
	if (led_brightness > MAX_BRIGHTNESS)
		led_brightness = MAX_BRIGHTNESS;
	
	byte bright_byte = (((led_brightness * self->brightness) / 31) & LED_BRIGHT_MASK) | LED_START;
	byte *start_ptr;
	for (int i = 0; i < self->num_led; i++) {
		start_ptr = self->leds + (i * BYTES_PER_LED);
		*(start_ptr) = bright_byte;
		*(start_ptr + self->rgb[RED]) = r;
		*(start_ptr + self->rgb[GRN]) = g;
		*(start_ptr + self->rgb[BLU]) = b;
	}
	
	Py_RETURN_NONE;
}

PyDoc_STRVAR(apa102_clear_strip_doc,
	"clear_strip()\n\n"
	"sets all pixels to black");
static PyObject * apa102_clear_strip(apa102Object *self, PyObject *args)
{
	
	for(int i = 0; i < self->num_led_array; i++) {
		if (i % BYTES_PER_LED != 0)
			*(self->leds+i) = 0;
	}
		
	Py_RETURN_NONE;
}


PyDoc_STRVAR(apa102_show_doc,
	"show()\n\n"
	"sends the data to the pixels to display");
static PyObject * apa102_show(apa102Object *self, PyObject *args)
{
	if (self->spi_write == NULL)
		Py_RETURN_NONE;
	
	Py_ssize_t end_bytes = (self->num_led + 15) / 16;
	PyObject *list = PyList_New(self->num_led_array + 4 + end_bytes);

	//first send out 4 0-bytes to tell the first pixel it's color will be sent next
	PyObject *zero_obj = PyLong_FromLong(0l);
	for (Py_ssize_t i = 0; i < 4; i++) {
		//set item takes ownership of the object
		//since I'm using it multiple times, I must increase the ref count each time
		Py_INCREF(zero_obj);
		PyList_SetItem(list, i, zero_obj);
		
	}
	
	PyObject *loopO;
	for (Py_ssize_t i = 0; i<self->num_led_array; i++) {
		loopO = PyLong_FromLong((long)(*(self->leds+i)));
		PyList_SetItem(list, i+4, loopO);
	}
	
	for (Py_ssize_t i = self->num_led_array+4; i < self->num_led_array+4+end_bytes; i++) {
		Py_INCREF(zero_obj);
		PyList_SetItem(list, i, zero_obj);
	}
	//there is one more ref count than it needs before this
	Py_DECREF(zero_obj);
	
	
	PyObject *tuple = PyTuple_Pack(1, list);
	PyObject *result = PyObject_CallObject(self->spi_write, tuple);
	if (result == NULL)
		return NULL;
	Py_DECREF(result);
	Py_DECREF(tuple);
	Py_DECREF(list);
	
	

	Py_RETURN_NONE;
	
}

static const char HEX_CHARS[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
static const char NUMBER_SIGN = '#', NULL_CHAR = '\0';

PyDoc_STRVAR(apa102_get_pixel_color_str_doc,
	"get_pixel_color_str(led_num)\n\n"
	"returns the color of the requested pixel as a string in the format \"RRGGBB\"\n"
	"or None if led_num is out of range");
static PyObject * apa102_get_pixel_color_str(apa102Object *self, PyObject *args) {
	int led_num;
	static const char *types = "i:get_pixel_color_str";
	PyArg_ParseTuple(args, types, &led_num);
	
	if(led_num < 0 || led_num > self->num_led)
		Py_RETURN_NONE;
	
	led_num *= BYTES_PER_LED;
	char str[8];
	str[0] = NUMBER_SIGN;
	str[7] = NULL_CHAR;
	byte b = 0;
	for (int i = 0; i < BYTES_PER_LED-1; i++) {
		b = *(self->leds+(led_num)+self->rgb[i]);
		str[i*2+2] = HEX_CHARS[b&15];
		str[i*2+1] = HEX_CHARS[(b>>4)&15];
	}
	PyObject* returnObj =  PyUnicode_FromString(&str[0]);
	return returnObj;
}

PyDoc_STRVAR(apa102_get_pixel_color_rgb_doc,
	"get_pixel_color_rgb(led_num)\n\n"
	"returns the color of the requested pixel as a long in the format 0xRRGGBB\n"
	"or None if led_num is out of range");
static PyObject * apa102_get_pixel_color_rgb(apa102Object *self, PyObject *args) {
	int led_num;
	static const char *types = "i:get_pixel_color_rgb";
	PyArg_ParseTuple(args, types, &led_num);
	if(led_num < 0 || led_num > self->num_led)
			Py_RETURN_NONE;
	
	byte r = *(self->leds + led_num*BYTES_PER_LED + self->rgb[RED]);
	byte g = *(self->leds + led_num*BYTES_PER_LED + self->rgb[GRN]);
	byte b = *(self->leds + led_num*BYTES_PER_LED + self->rgb[BLU]);
	long color = (r << 16) | (g << 8) | b;
	return PyLong_FromLong(color);
}
PyDoc_STRVAR(apa102_get_pixel_color_doc,
	"get_pixel_color_tuple(led_num)\n\n"
	"returns the color of the requested pixel as a tuple of 3 numbers in the format (R, G, B)\n"
	"or None if led_num is out of range");
static PyObject * apa102_get_pixel_color(apa102Object *self, PyObject *args) {
	int led_num;
	static const char *types = "i:get_pixel_color";
	PyArg_ParseTuple(args, types, &led_num);
	if(led_num < 0 || led_num > self->num_led)
			Py_RETURN_NONE;
	
	byte r = *(self->leds + led_num*BYTES_PER_LED + self->rgb[RED]);
	byte g = *(self->leds + led_num*BYTES_PER_LED + self->rgb[GRN]);
	byte b = *(self->leds + led_num*BYTES_PER_LED + self->rgb[BLU]);
	PyObject* tuple = PyTuple_New(3);
	PyTuple_SetItem(tuple, 0, PyLong_FromLong((long)r));
	PyTuple_SetItem(tuple, 1, PyLong_FromLong((long)g));
	PyTuple_SetItem(tuple, 2, PyLong_FromLong((long)b));
	return tuple;
}

PyDoc_STRVAR(apa102_combine_color_doc,
	"combine_color(red, green, blue)\n\n"
	"sets all pixels to show the given red, green, and blue values (from 0 to 255 inclusive)\n"
	"optional- include a brightness to display the pixels at (from 0 to 31 inclusive)");
static PyObject * apa102_combine_color(apa102Object *self, PyObject *args, PyObject *keywds) {
		static char *kwlist[] = {"red", "green", "blue", NULL};
		static const char *types = "bbb:combine_color";	
		unsigned char r, g, b;
		PyArg_ParseTupleAndKeywords(args, keywds, types, kwlist, &r, &g, &b);
		long rgb = (r << 16) | (g << 8) | b;
		return PyLong_FromLong(rgb);
}

PyDoc_STRVAR(apa102_rotate_doc,
	"rotate([positions=1))\n\n"
	"rotates the pixels by the given amount\n"
	"positions can be negative to rotate backwards");
static PyObject * apa102_rotate(apa102Object *self, PyObject *args) {
		static const char *types = "|i:rotate";	
		int signed_pos = 1;
		PyArg_ParseTuple(args, types, &signed_pos);
		
		//make sure number of shifts is less than the number of leds
		signed_pos %= self->num_led;
		
		if (signed_pos == 0)
			Py_RETURN_NONE;
		
		int8_t direction = (signed_pos < 0) ? -1 : 1;
		int pos = abs(signed_pos);
		
		//if it takes less work to shift in the other direction, do that instead
		if (pos > self->num_led/2) {
			pos = self->num_led - signed_pos;
			direction *= -1;
		}
		
		pos *= BYTES_PER_LED;
		int *temp = PyMem_Calloc(pos, sizeof(byte));
		
		if (direction > 0) {
			//store first [pos] elements in an array
			for (int i = 0; i < pos; i++) {
				*(temp+i) = *(self->leds+i);
			}
			//shift the other values over:
			for (int i = 0; i < self->num_led_array-pos; i++) {
				*(self->leds+i) = *(self->leds+i+pos);
			}
			//put the temp values in the end of the array
			for (int i = 0; i < pos; i++) {
				*(self->leds+self->num_led_array-pos+i) = *(temp+i);
			}
		}
		else {
			//store last [pos] elements in an array
			for (int i = 0; i < pos; i++) {
				*(temp+i) = *(self->leds+self->num_led_array-pos+i);
			}
			//shift the other values over:
			for (int i = self->num_led_array-1; i > pos; i--) {
				*(self->leds+i) = *(self->leds+i-pos);
			}
			//put the temp values in the begining of the array
			for (int i = 0; i < pos; i++) {
				*(self->leds+i) = *(temp+i);
			}
		}
		PyMem_Free(temp);
		Py_RETURN_NONE;
}

PyDoc_STRVAR(apa102_wheel_doc,
	"wheel(wheel_pos)\n\n"
	"Get a color from a color wheel that cycles in the order Green -> Red -> Blue -> Green...");
static PyObject * apa102_wheel(apa102Object *self, PyObject *args) {
		static const char *types = "b:wheel";	
		unsigned char pos = 1;
		PyArg_ParseTuple(args, types, &pos);
		//Get a color from a color wheel; Green -> Red -> Blue -> Green
		long rgb;
		if (pos < 85){  //Green -> Red
			pos *= 3;
			rgb = (pos << 16) | ((255-pos) << 8);
		}
		else if (pos < 170){  // Red -> Blue
			pos = (pos - 85)*3;
			rgb = ((255-pos) << 16) | pos;
		}
		else { // Blue -> Green
			pos = (pos-170)*3;
			rgb = (pos << 8) | (255-pos);
		}
		return PyLong_FromLong(rgb);
}

static const char OPEN_BRACKET = '[', CLOSED_BRACKET = ']', SPACE = ' ', COMMA = ',';
PyDoc_STRVAR(apa102_dump_array_doc,
	"dump_array()\n\n"
	"For debug purposes: Dump the LED array onto the console.");
static PyObject * apa102_dump_array(apa102Object *self, PyObject *args) {
		int length = (self->num_led_array*4);
		char *text = PyMem_Calloc(length, sizeof(char));
		*text = OPEN_BRACKET;
		*(text+length-1) = CLOSED_BRACKET;
		int offset = 1;
		byte b;
		for(int i = 0; i < self->num_led_array-1; i++) {
			b = *(self->leds+i);
			*(text+offset) = HEX_CHARS[b&15];
			*(text+offset+1) = HEX_CHARS[(b>>4)&15];
			*(text+offset+2) = COMMA;
			*(text+offset+3) = SPACE;
			offset += 4;
		}
		b = *(self->leds+self->num_led_array-1);
		*(text+length-3) = HEX_CHARS[b&15];
		*(text+length-2) = HEX_CHARS[(b>>4)&15];
		printf("%s", text);
		PyMem_Free(text);
		Py_RETURN_NONE;
}

static PyMethodDef apa102_methods[] = {
	{"set_pixel",				(PyCFunction)apa102_set_pixel,				METH_VARARGS | METH_KEYWORDS,	apa102_set_pixel_doc},
	{"set_pixel_rgb",			(PyCFunction)apa102_set_pixel_rgb,			METH_VARARGS | METH_KEYWORDS,	apa102_set_pixel_rgb_doc},
	{"set_range",				(PyCFunction)apa102_set_range,				METH_VARARGS | METH_KEYWORDS,	apa102_set_range_doc},
	{"set_range_rgb",			(PyCFunction)apa102_set_range_rgb,			METH_VARARGS | METH_KEYWORDS,	apa102_set_range_rgb_doc},
	{"set_all",					(PyCFunction)apa102_set_all,				METH_VARARGS | METH_KEYWORDS,	apa102_set_all_doc},
	{"set_all_rgb",				(PyCFunction)apa102_set_all_rgb,			METH_VARARGS | METH_KEYWORDS,	apa102_set_all_rgb_doc},
	{"show",					(PyCFunction)apa102_show,					METH_VARARGS, 					apa102_show_doc},
	{"clear_strip",				(PyCFunction)apa102_clear_strip,			METH_VARARGS,  					apa102_clear_strip_doc},
	{"rotate",					(PyCFunction)apa102_rotate,					METH_VARARGS,  					apa102_rotate_doc},
	{"get_pixel_color_str",		(PyCFunction)apa102_get_pixel_color_str,	METH_VARARGS,  					apa102_get_pixel_color_str_doc},
	{"get_pixel_color_rgb",		(PyCFunction)apa102_get_pixel_color_rgb,	METH_VARARGS,  					apa102_get_pixel_color_rgb_doc},
	{"get_pixel_color",			(PyCFunction)apa102_get_pixel_color,		METH_VARARGS,  					apa102_get_pixel_color_doc},
	{"combine_color",			(PyCFunction)apa102_combine_color,			METH_VARARGS | METH_KEYWORDS,	apa102_combine_color_doc},
	{"wheel",					(PyCFunction)apa102_wheel,					METH_VARARGS,					apa102_wheel_doc},
	{"dump_array",				(PyCFunction)apa102_dump_array,				METH_VARARGS,					apa102_dump_array_doc},
	{NULL, NULL, 0, NULL}           /* sentinel */
};

PyDoc_STRVAR(apa_num_led_var_doc, "the number of leds in this strip");
static PyObject * apa102_get_num_led(apa102Object *self, void *closure)
{
	PyObject *result = PyLong_FromLong((long)self->num_led);
	//I thought I should Py_INCREF the result before returning it, but that caused a memory leak
	return result;
}

PyDoc_STRVAR(apa_global_brightness_var_doc, "how bright this strip displays from 0 to MAX_BRIGHTNESS (inclusive)");
static PyObject * apa102_get_global_brightness(apa102Object *self, void *closure) {
	PyObject *result = PyLong_FromLong((long)self->brightness);
	return result;
}

PyDoc_STRVAR(apa_order_var_doc, "the order red, green, and blue values are sent to the strip (ex: an order of RGB={1,2,3}");
static PyObject * apa102_get_order(apa102Object *self, void *closure) {
	PyObject *result = PyList_New(3);
	PyList_SetItem(result, RED, PyLong_FromLong((long)(self->rgb[RED])));
	PyList_SetItem(result, GRN, PyLong_FromLong((long)(self->rgb[GRN])));
	PyList_SetItem(result, BLU, PyLong_FromLong((long)(self->rgb[BLU])));
	//I thought I should Py_INCREF the result before returning it, but that caused a memory leak
	return result;
}

PyDoc_STRVAR(apa_write_callback_var_doc, "the function called to write the data to the leds");
static PyObject * apa102_get_write_callback(apa102Object *self, void *closure) {
	if (self->spi_write == NULL)
		Py_RETURN_NONE;
	else
		return self->spi_write;
}

PyDoc_STRVAR(apa_max_brightness_var_doc, "the maximum that the brightness value can be set to");
static PyObject * apa102_get_max_brightness(apa102Object *self, void *closure) {
	PyObject *result = PyLong_FromLong((long)MAX_BRIGHTNESS);
	return result;
}

static PyGetSetDef apa102_getset[] = {
	{"num_led", 			(getter)apa102_get_num_led,				0,	apa_num_led_var_doc},
	{"global_brightness",	(getter)apa102_get_global_brightness,	0,	apa_global_brightness_var_doc},
	{"order", 				(getter)apa102_get_order,				0,	apa_order_var_doc},
	{"write_callback",		(getter)apa102_get_write_callback,		0,	apa_write_callback_var_doc},
	{"MAX_BRIGHTNESS", 		(getter)apa102_get_max_brightness,		0,	apa_max_brightness_var_doc},
	{NULL},
};


static PyTypeObject apa102_Type = {
	/* The ob_type field must be initialized in the module init function
	 * to be portable to Windows without using C++. */
	PyVarObject_HEAD_INIT(NULL, 0)
	"apa102.APA102",            /*tp_name*/
	sizeof(apa102Object),       /*tp_basicsize*/
	0,                          /*tp_itemsize*/
	/* methods */
	(destructor)apa102_dealloc, /*tp_dealloc*/
	0,                          /*tp_print*/
	0,             /*tp_getattr*/
	0,/*tp_setattr*/
	0,                          /*tp_reserved*/
	0,                          /*tp_repr*/
	0,                          /*tp_as_number*/
	0,                          /*tp_as_sequence*/
	0,                          /*tp_as_mapping*/
	0,                          /*tp_hash*/
	0,                          /*tp_call*/
	0,                          /*tp_str*/
	0, /*tp_getattro*/
	0,                          /*tp_setattro*/
	0,                          /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,         /*tp_flags*/
	0,                          /*tp_doc*/
	0,                          /*tp_traverse*/
	0,                          /*tp_clear*/
	0,                          /*tp_richcompare*/
	0,                          /*tp_weaklistoffset*/
	0,                          /*tp_iter*/
	0,                          /*tp_iternext*/
	apa102_methods,             /*tp_methods*/
	0,                          /*tp_members*/
	apa102_getset,              /*tp_getset*/
	0,                          /*tp_base*/
	0,                          /*tp_dict*/
	0,                          /*tp_descr_get*/
	0,                          /*tp_descr_set*/
	0,                          /*tp_dictoffset*/
	(initproc)apa102_init,      /*tp_init*/
	0,                          /*tp_alloc*/
	PyType_GenericNew,          /*tp_new*/
	0,                          /*tp_free*/
	0,                          /*tp_is_gc*/
};


static int apa102_exec(PyObject *m)
{
	/* Slot initialization is subject to the rules of initializing globals.
	   C99 requires the initializers to be "address constants".  Function
	   designators like 'PyType_GenericNew', with implicit conversion to
	   a pointer, are valid C99 address constants.
	   However, the unary '&' operator applied to a non-static variable
	   like 'PyBaseObject_Type' is not required to produce an address
	   constant.  Compilers may support this (gcc does), MSVC does not.
	   Both compilers are strictly standard conforming in this particular
	   behavior.
	*/
	apa102_Type.tp_base = &PyBaseObject_Type;
	/* Finalize the type object including setting type of the new type
	 * object; doing it here is required for portability, too. */
	if (PyType_Ready(&apa102_Type) < 0)
		goto fail;

	/* Add some symbolic constants to the module */
	if (ErrorObject == NULL) {
		ErrorObject = PyErr_NewException("APA102.error", NULL, NULL);
		if (ErrorObject == NULL)
			goto fail;
	}
	Py_INCREF(ErrorObject);
	PyModule_AddObject(m, "error", ErrorObject);
	
	if (PyType_Ready(&apa102_Type) < 0)
		goto fail;
	Py_INCREF(&apa102_Type);
	PyModule_AddObject(m, "APA102", (PyObject *)&apa102_Type);
	return 0;
 fail:
	Py_XDECREF(m);
	return -1;
}

static struct PyModuleDef_Slot apa102_slots[] = {
	{Py_mod_exec, apa102_exec},
	{0, NULL},
};

static struct PyModuleDef apa102module = {
	PyModuleDef_HEAD_INIT,
	"apa102",
	apa102_module_doc,
	sizeof(apa102Object),
	apa102_methods,
	apa102_slots,
	NULL,
	NULL,
	NULL
};

/* Export function for the module (*must* be called PyInit_apa102) */

PyMODINIT_FUNC
PyInit_apa102(void)
{
	return PyModuleDef_Init(&apa102module);
}