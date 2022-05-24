#ifndef KEYBOARD_BINDINGS_H
#define KEYBOARD_BINDINGS_H

#include <SDL2/SDL.h>

//// global keybindings - functional regarless of mode ////

uint8_t keybind_global_file_save(SDL_Event evt){
  if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_s && SDL_GetModState() & KMOD_CTRL){
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_global_file_reload(SDL_Event evt){
  if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_r && SDL_GetModState() & KMOD_CTRL){
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_viewport_mode_toggle(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_TAB && evt.type == SDL_KEYDOWN){
    return TRUE;
  }
  return FALSE;
}

//// editor mode keybindings ////


//// display mode keybindings ////

uint8_t keybind_display_camera_time_zoom_in(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_EQUALS){ //the plus key on us keyboards
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_camera_time_zoom_all(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_HOME){
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_camera_time_zoom_out(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_MINUS){ 
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_camera_time_scroll_up(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_u){ 
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_camera_time_scroll_down(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_i){ 
    return TRUE;
  }
  return FALSE;
}


uint8_t keybind_display_select_prereq_one(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_w){ 
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_select_prereq_all(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_w && SDL_GetModState() & KMOD_SHIFT){ 
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_select_none(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_SPACE){
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_task_create_split(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_x && evt.type == SDL_KEYDOWN){
    return TRUE;
  }
  return FALSE;
}

uint8_t keybind_display_task_create_successor(SDL_Event evt){
  if (evt.key.keysym.sym == SDLK_a && evt.type == SDL_KEYDOWN){
    return TRUE;
  }
  return FALSE;
}

#endif
