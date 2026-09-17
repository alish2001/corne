if(CONFIG_CORNE_DIAGNOSTICS)
  zephyr_library()
  zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_library_sources(src/diagnostics.c)
  # Wrap external call sites without modifying the pinned upstream sources.
  zephyr_link_libraries(
    -Wl,--wrap=zmk_event_manager_raise
    -Wl,--wrap=bt_gatt_notify_cb
    -Wl,--wrap=z_impl_k_msgq_put
  )
  if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_link_libraries(-Wl,--wrap=zmk_hog_send_keyboard_report)
  endif()
endif()
