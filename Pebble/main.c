/*
 * WatchDog program for Pebble Watch.
 * Authors: Ryan Smith, Theresa Breiner and Brad Thompson
 * Version: April 2015
 * Citation: The initial skelton for basic send/receive handlers 
 *           was provided by Dr. Chris Murphy at the University of Pennsylvania.
 */
#include <pebble.h>

static Window *window;
static TextLayer *hello_layer;  // refers to the TextLayer of the watchface.
static char msg[100];           // used to hold messages returned from the JavaScript.
int wantAverage = 0;            // indicates that we wish to continuously display the most recent temperature.
int standbyEngaged = 0;         // indicates that standby is engaged on the Arduino.
int tripped = 0;                // indicates that the motion sensor has been tripped.
int tickTimerMod = 0;           // used to alternate between requests in the ticker timer.

/* 
 * Confims outgoing message was delivered. - Nothing to be done.
 */
void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered -- do nothing
 }

/* 
 * Displays an error message if outgoing message failed to send. This occurs when connection with the
 * middleware(phone) is unsuccessful (bluetooth turned off, etc.).
 */
 void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
   text_layer_set_text(hello_layer, "Middleware Error!");
 }

/* 
 * Receives a message returned from the JavaScript following a request to the server. 
 * If the message is "tripped" - sets var tripped to 1.
 * If message is "nottripped" - it is ignored.
 * Otherwise - the returned message is displayed on the Pebble watchface.
 */
 void in_received_handler(DictionaryIterator *received, void *context) {
    // looks for key #0 in the incoming message
   int key = 0;
   Tuple *text_tuple = dict_find(received, key);
   if (text_tuple) {
     if (text_tuple->value) {
       // put it in this global variable
       strcpy(msg, text_tuple->value->cstring);
     }
     else strcpy(msg, "no value!");
     //at this point we hav copied the returned message into msg
     //check to see if the alarm was tripped
     if (strncmp(msg, "tripped", 7) == 0){
         tripped = 1;
     }
     else if (strncmp(msg, "nottripped", 7) == 0){
       //do nothing - no display and no alarm
     }
     else{
        text_layer_set_text(hello_layer, msg);
     }
   }
   else {
     text_layer_set_text(hello_layer, "no message!");
   }
 }

/* 
 * Displays an error message if incoming message is dropped.
 */
 void in_dropped_handler(AppMessageResult reason, void *context) {
   text_layer_set_text(hello_layer, "Error in!");
 }

/* 
 * Responds to select(center) button clicks. When !tripped, sends a message requesting
 * most recent temperature reading to the server. When tripped, sends a message
 * requesting the server make the Arduino display the "GET OUT!" message.
 */
void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!tripped){
      wantAverage = 1;
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      int key = 0;
      // send the message "b" to the phone, using key #0
      Tuplet value = TupletCString(key, "b");
      dict_write_tuplet(iter, &value);
      app_message_outbox_send();
  }
  else {
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      int key = 0;
      // send the message "b" to the phone, using key #0
      Tuplet value = TupletCString(key, "m");
      dict_write_tuplet(iter, &value);
      app_message_outbox_send();
  }
}

/* 
 * Responds to up button clicks. When !tripped, sends a message requesting
 * that the server change the temperature reading from c->F or vice versa on both the
 * watchface and the Arduino display.
 */
void up_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  wantAverage = 1;
   //text_layer_set_text(hello_layer, "UP!");
  DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
   int key = 0;
   // send the message "hello?" to the phone, using key #0
   Tuplet value = TupletCString(key, "a");
   dict_write_tuplet(iter, &value);
   app_message_outbox_send();
}

/* 
 * Responds to down button clicks. When !tripped, sends a message requesting
 * the max, min and average temperatures to the server.
 */
void down_select_click_handler(ClickRecognizerRef recognizer, void *context) {
   wantAverage = 0;
   DictionaryIterator *iter;
   app_message_outbox_begin(&iter);
   int key = 0;
   // send the message "d" to the phone, using key #0
   Tuplet value = TupletCString(key, "d");
   dict_write_tuplet(iter, &value);
   app_message_outbox_send();
}

/* 
 * Responds to double up button clicks. When !tripped, asks the server to put the Arduino in
 * or out of standby mode and records it. When tripped, resets the alarm on the server and Arduino.
 */
void up_double_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!tripped){
      standbyEngaged = !standbyEngaged;
      wantAverage = 0;
      //text_layer_set_text(hello_layer, "UP!");
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      int key = 0;
      // send the message "hello?" to the phone, using key #0
      Tuplet value = TupletCString(key, "s");
      dict_write_tuplet(iter, &value);
      app_message_outbox_send();
  }
  else {
      tripped = 0;
      //text_layer_set_text(hello_layer, "UP!");
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      int key = 0;
      // send the message "hello?" to the phone, using key #0
      Tuplet value = TupletCString(key, "r");
      dict_write_tuplet(iter, &value);
      app_message_outbox_send();
  }
}

/*
 * Tick-Handler - executes once every second using watch's internal clock.
 * Switched every other second between getting most recent temp (if desired) and checking for tripped alarm.
 */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (tickTimerMod % 2 == 0){
      if (wantAverage && !standbyEngaged && !tripped){
        DictionaryIterator *iter;
        app_message_outbox_begin(&iter);
        int key = 0;
        // send the message "b" to the phone, using key #0
        Tuplet value = TupletCString(key, "b");
        dict_write_tuplet(iter, &value);
        app_message_outbox_send();
      }
  }
  else{
      //checks if alarm has been tripped, checks to see every 10 seconds if not
      if (tripped){
          text_layer_set_text(hello_layer, "INTRUDER ALERT!!!");
          vibes_double_pulse();
      }
      else { 
          DictionaryIterator *iter;
          app_message_outbox_begin(&iter);
          int key = 0;
          // send the message "t" to the phone, using key #0
          Tuplet value = TupletCString(key, "t");
          dict_write_tuplet(iter, &value);
          app_message_outbox_send();
      }
  }
  tickTimerMod += 1;
}

/* 
 * Registers the appropriate function to the appropriate button. 
 */
void config_provider(void *context) {
   window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
   window_single_click_subscribe(BUTTON_ID_UP, up_select_click_handler);
   window_multi_click_subscribe(BUTTON_ID_UP, 2, 2, 500, true, up_double_click_handler);
   window_single_click_subscribe(BUTTON_ID_DOWN, down_select_click_handler);
}

/* 
 * Basic setup operations that take place upon loading the program.
 */
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  hello_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(hello_layer, "Welcome to WatchDog");
  text_layer_set_text_alignment(hello_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(hello_layer));
}

/* 
 * Tears down the watchface upon exiting the program.
 */
static void window_unload(Window *window) {
  text_layer_destroy(hello_layer);
}

/* 
 * Initializes many basic settings.
 */
static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  //sets up tick handlers for items attached to clock
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  
  // need this for adding the listener
  window_set_click_config_provider(window, config_provider);
   // for registering AppMessage handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  const bool animated = true;
  window_stack_push(window, animated);
}

/* 
 * Tears down initialization settings.
 */
static void deinit(void) {
  window_destroy(window);
}

/* 
 * Main driver - begins the program.
 */
int main(void) {
  init();
  app_event_loop();
  deinit();
}