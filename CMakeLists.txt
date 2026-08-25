# Add the Bluetooth-profile RGB indicator to the central half only.
if(CONFIG_ZMK_HELIX_PROFILE_RGB)
  if((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    target_sources(app PRIVATE src/profile_rgb.c)
  endif()
endif()
