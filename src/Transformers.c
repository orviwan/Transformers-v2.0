#include "pebble.h"
#include "num2words-en.h"

#define THRESHOLD (775)   // mg

#define BUFFER_SIZE 44
#define TOTAL_FRAMES 15

static Window *window;

static GBitmap *logo_image;
static BitmapLayer *logo_layer;

static AppTimer *timer;

static int current_frame = 0;
static int direction = 0;  //0=forward / 1=backward
static uint32_t delta = 8;        // FPS
static bool animating = false;

typedef struct {
    TextLayer *currentLayer;
    TextLayer *nextLayer;	
    PropertyAnimation *currentAnimation;
    PropertyAnimation *nextAnimation;
} Line;

static Line line1;
static Line line2;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];

static GFont lightFont;
static GFont boldFont;

const int IMAGE_ANIM_RESOURCE_IDS[] = {
    RESOURCE_ID_IMAGE_ANIM_1,
    RESOURCE_ID_IMAGE_ANIM_2,
    RESOURCE_ID_IMAGE_ANIM_3,
    RESOURCE_ID_IMAGE_ANIM_4,
    RESOURCE_ID_IMAGE_ANIM_5,
    RESOURCE_ID_IMAGE_ANIM_6,
    RESOURCE_ID_IMAGE_ANIM_7,
    RESOURCE_ID_IMAGE_ANIM_8,
    RESOURCE_ID_IMAGE_ANIM_9,
    RESOURCE_ID_IMAGE_ANIM_10,
    RESOURCE_ID_IMAGE_ANIM_11,
    RESOURCE_ID_IMAGE_ANIM_12,
    RESOURCE_ID_IMAGE_ANIM_13,
    RESOURCE_ID_IMAGE_ANIM_14,
    RESOURCE_ID_IMAGE_ANIM_15
};

static bool flip_detected(AccelData *data, uint32_t num_samples);
bool isSpace(char c);
char *trim(char *input);
static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin);
static void handle_accel_data(AccelData *data, uint32_t num_samples);

void update_logo() {

	if(direction==1) {  //backwards
        current_frame--;
    }
    else {
        current_frame++; //forwards
    }

	
    GPoint point;
    if(current_frame > 8) {
        point = GPoint(17, 8);
    }
    else {
        point = GPoint(17, 0);
    }

	if(current_frame>=TOTAL_FRAMES || current_frame<0) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "update_logo(): stopping");
		app_timer_cancel(timer);
		animating = false;
        return;		
    }
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "update_logo(): current_frame: %d, direction: %d", current_frame, direction);
    set_container_image(&logo_image, logo_layer, IMAGE_ANIM_RESOURCE_IDS[current_frame], point);
	


}



static void timer_callback(void *data) {        
	
    update_logo();	

	if(current_frame>=TOTAL_FRAMES || current_frame<=0) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "timer_callback(): cancelling timer");
        app_timer_cancel(timer);
		animating = false;
		accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
	    accel_data_service_subscribe(25, &handle_accel_data);   
		return;
	}

	//APP_LOG(APP_LOG_LEVEL_DEBUG, "timer_callback(): starting timer again");
	timer = app_timer_register(delta, (AppTimerCallback) timer_callback, 0);


}

static void start() {

	if(animating) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "START(): Already running");
		return;
	}
	animating = true;
	accel_data_service_unsubscribe(); 
	vibes_long_pulse();
	
    if(current_frame>=TOTAL_FRAMES) {
      direction = 1;
    }
    if(current_frame==0) {
      direction = 0;
    }
	
	
	
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "start(): starting timer");
    timer = app_timer_register(delta, (AppTimerCallback) timer_callback, 0);
}

static void handle_accel_data(AccelData *data, uint32_t num_samples) {
    if (animating || !flip_detected(data, num_samples)) {
        return;
    }
	APP_LOG(APP_LOG_LEVEL_DEBUG, "FLIP! Animating: %s", animating ? "true" : "false");
    start();    
}

// Animation handler
static void handle_text_animation_stopped(struct Animation *animation, bool finished, void *context)
{
	Layer *textLayer = text_layer_get_layer((TextLayer *)context);
	GRect rect = layer_get_frame(textLayer);
	rect.origin.x = 144;
	layer_set_frame(textLayer, rect);
}

// Animate line
static void text_animate_layer(Line *line, TextLayer *current, TextLayer *next)
{
	GRect fromRect = layer_get_frame(text_layer_get_layer(next));
	GRect toRect = fromRect;
	toRect.origin.x -= 144;
	
	line->nextAnimation = property_animation_create_layer_frame(text_layer_get_layer(next), &fromRect, &toRect);
	animation_set_duration((Animation *)line->nextAnimation, 400);
	animation_set_curve((Animation *)line->nextAnimation, AnimationCurveEaseOut);
	animation_schedule((Animation *)line->nextAnimation);
	
	GRect fromRect2 = layer_get_frame(text_layer_get_layer(current));
	GRect toRect2 = fromRect2;
	toRect2.origin.x -= 144;
	
	line->currentAnimation = property_animation_create_layer_frame(text_layer_get_layer(current), &fromRect2, &toRect2);
	animation_set_duration((Animation *)line->currentAnimation, 400);
	animation_set_curve((Animation *)line->currentAnimation, AnimationCurveEaseOut);
	
	animation_set_handlers((Animation *)line->currentAnimation, (AnimationHandlers) {
		.stopped = (AnimationStoppedHandler)handle_text_animation_stopped
	}, current);
	
	animation_schedule((Animation *)line->currentAnimation);
}

static void text_update_text(Line *line, char lineStr[2][BUFFER_SIZE], char *value)
{
	TextLayer *next, *current;
	
	GRect rect = layer_get_frame(text_layer_get_layer(line->currentLayer));
	current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
	next = (current == line->currentLayer) ? line->nextLayer : line->currentLayer;
	
	// Update correct text only
	if (current == line->currentLayer) {;
		text_layer_set_text(next, value);
	} else {
		text_layer_set_text(next, value);
	}
	
	text_animate_layer(line, current, next);
}

static void configure_layer1(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, boldFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentCenter);
}

static void configure_layer2(TextLayer *textlayer)
{
	text_layer_set_font(textlayer, lightFont);
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_text_alignment(textlayer, GTextAlignmentCenter);
}


static void update_hours(int hour)
{
	static char textLine1[40];
	if(hour > 12) hour -= 12;
	if(hour==0) hour = 12;
	number_to_words(hour, textLine1);	
	text_update_text(&line1, line1Str, textLine1);	
}

static void update_minutes(int min)
{
	static char textLine2[40];
	number_to_words(min, textLine2);	
	text_update_text(&line2, line2Str, textLine2);	
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {	  
	if(units_changed & HOUR_UNIT) {	
		update_hours(tick_time->tm_hour);
	}
	if(units_changed & MINUTE_UNIT) {
		update_minutes(tick_time->tm_min);
	}
}

static void init() {
    window = window_create();
    window_stack_push(window, true);
    window_set_background_color(window, GColorBlack);
	Layer *window_layer = window_get_root_layer(window);
	
    logo_image = gbitmap_create_with_resource(IMAGE_ANIM_RESOURCE_IDS[current_frame]);
    GRect frame = (GRect) {
        .origin = { .x = 17, .y = 0 },
		.size = { .w = 110, .h = 123 }
    };
    logo_layer = bitmap_layer_create(frame);
    bitmap_layer_set_bitmap(logo_layer, logo_image);
    layer_add_child(window_layer, bitmap_layer_get_layer(logo_layer));  	
    
	// Custom fonts
	lightFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TRANSFORMERS_20));
	boldFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TRANSFORMERS_30));

	// 1st line layers
	line1.currentLayer = text_layer_create(GRect(0, 114, 144, 40)); 
	line1.nextLayer = text_layer_create(GRect(144, 114, 144, 50));
	configure_layer1(line1.currentLayer);
	configure_layer1(line1.nextLayer);

	// 2nd layers
	line2.currentLayer = text_layer_create(GRect(0, 148, 144, 30));  
	line2.nextLayer = text_layer_create(GRect(144, 148, 144, 30));
	configure_layer2(line2.currentLayer);
	configure_layer2(line2.nextLayer);

	// Load layers
	layer_add_child(window_layer, text_layer_get_layer(line1.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line1.nextLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.currentLayer));
	layer_add_child(window_layer, text_layer_get_layer(line2.nextLayer));

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    handle_minute_tick(current_time, HOUR_UNIT+MINUTE_UNIT);    

    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

    accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
    accel_data_service_subscribe(25, &handle_accel_data);
}

static void deinit() {
    tick_timer_service_unsubscribe();
    accel_data_service_unsubscribe();
	
	app_timer_cancel(timer);
	timer = NULL;

	fonts_unload_custom_font(lightFont);
	fonts_unload_custom_font(boldFont);
	
	layer_remove_from_parent(text_layer_get_layer(line1.currentLayer));
	layer_remove_from_parent(text_layer_get_layer(line1.nextLayer));
	layer_remove_from_parent(text_layer_get_layer(line2.currentLayer));
	layer_remove_from_parent(text_layer_get_layer(line2.nextLayer));
	
	text_layer_destroy(line1.currentLayer);
	text_layer_destroy(line1.nextLayer);	
	text_layer_destroy(line2.currentLayer);
	text_layer_destroy(line2.nextLayer);
	
	property_animation_destroy(line1.currentAnimation);
	property_animation_destroy(line1.nextAnimation);
	property_animation_destroy(line2.currentAnimation);
	property_animation_destroy(line2.nextAnimation);

    layer_remove_from_parent(bitmap_layer_get_layer(logo_layer));
    bitmap_layer_destroy(logo_layer);
    gbitmap_destroy(logo_image);	
	logo_image = NULL;

    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}

bool flip_detected(AccelData *data, uint32_t num_samples) {
  uint8_t i = 0;

  // Look for +y threshold
  while (i < num_samples) {
    if ((data + i)->y > THRESHOLD) {
      break;
    }
    ++i;
  }

  if (i == num_samples) {
    return (false);
  }

  // Look for -z threshold
  while (i < num_samples) {
    if ((data + i)->z < -THRESHOLD) {
      return (true);
    }

    ++i;
  }

  return (false);
}


static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
    GBitmap *old_image = *bmp_image;
    *bmp_image = gbitmap_create_with_resource(resource_id);
    GRect frame = (GRect) {
        .origin = origin
    };
    bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
    //layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);
    if(old_image!=NULL) {
        gbitmap_destroy(old_image);
		old_image = NULL;
    }
}

