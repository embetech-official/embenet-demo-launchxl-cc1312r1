function (verify_simplelink_instance RESULT PATH)
  find_path(SIMPLELINK_SDK NAMES "DeviceFamily.h" PATHS "${PATH}" PATH_SUFFIXES "source/ti/devices"
                                                                                NO_CACHE
            NO_DEFAULT_PATH
  )
  set(${RESULT} ${SIMPLELINK_SDK} PARENT_SCOPE)
endfunction ()

function (find_simplelink_sdk SIMPLELINK_INSTANCE)

  set(FOUND_INSTANCES)

  # Get information about installed Code Composer Studio instances
  cmake_host_system_information(
    RESULT TI_CCS_KEYS QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Texas Instruments" SUBKEYS
  )
  list(FILTER TI_CCS_KEYS INCLUDE REGEX "Code Composer Studio*")
  list(SORT TI_CCS_KEYS ORDER DESCENDING)
  foreach (CCS_INSTANCE IN LISTS TI_CCS_KEYS)
    get_filename_component(
      CCS_INSTANCE_PATH
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Texas Instruments\\${CCS_INSTANCE};Location]" DIRECTORY
    )

    file(GLOB SIMPLELINK_CCS_INSTANCES LIST_DIRECTORIES true
         "${CCS_INSTANCE_PATH}/simplelink_cc13xx_cc26xx_sdk*"
         "${CCS_INSTANCE_PATH}/*/simplelink_cc13xx_cc26xx_sdk*"
    )

    list(APPEND FOUND_INSTANCES ${SIMPLELINK_CCS_INSTANCES})
  endforeach ()

  # Get information about installed Standalone Simplelink SDK instances
  cmake_host_system_information(
    RESULT TI_SDK_KEYS
    QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall"
          SUBKEYS
  )
  list(FILTER TI_SDK_KEYS INCLUDE REGEX "simplelink_cc13xx_cc26xx_sdk*")

  foreach (KEY IN LISTS TI_SDK_KEYS)
    get_filename_component(
      INSTANCE
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${KEY};UninstallString]"
      DIRECTORY
    )
    string(SUBSTRING "${INSTANCE}" 1 -1 INSTANCE) # leading \" (IDK why cmake # keeps appending it)
    if (EXISTS "${INSTANCE}")
      list(APPEND FOUND_INSTANCES ${INSTANCE})
      break()
    endif ()
  endforeach ()

  list(REMOVE_DUPLICATES FOUND_INSTANCES)
  list(SORT FOUND_INSTANCES ORDER DESCENDING)
  # Check if instances actually exist, then return the first (most recent)
  foreach (INSTANCE IN LISTS FOUND_INSTANCES)
    verify_simplelink_instance(VALID_INSTANCE "${INSTANCE}")
    if (VALID_INSTANCE)
      message(DEBUG "Found Simplelink SDK at ${VALID_INSTANCE}")
      set(${SIMPLELINK_INSTANCE} "${INSTANCE}" PARENT_SCOPE)
      break()
    endif ()
  endforeach ()

endfunction ()

if (NOT ENV{simplelink_cc13xx_cc26xx_sdk_DIR})
  find_simplelink_sdk(SDK_INST)
  if (NOT SDK_INST)
    message(FATAL_ERROR "simplelink_sdk not found")
  endif ()
  set(ENV{simplelink_cc13xx_cc26xx_sdk_DIR} ${SDK_INST})
  set(simplelink_cc13xx_cc26xx_sdk_DIR ${SDK_INST})
endif ()

if (NOT ENV{ti_sysconfig_bin})
  # Sysconfig comes with simplelink sdk, so check most probable path
  file(GLOB SYSCONFIG_DIRS LIST_DIRECTORIES true
       "$ENV{simplelink_cc13xx_cc26xx_sdk_DIR}/../sysconfig*"
  )

  find_file(SYSCONFIG_BIN sysconfig_cli.bat PATHS ${SYSCONFIG_DIRS} NO_DEFAULT_PATH REQUIRED)

  set(ENV{ti_sysconfig_bin} ${SYSCONFIG_BIN})
  set(ti_sysconfig_bin ${SYSCONFIG_BIN})
endif ()

function (simplelink_sdk_target target)
  set(oneValueArgs DESTINATION SCRIPT)
  cmake_parse_arguments(SDK_OPT "" "${oneValueArgs}" "" ${ARGN})

  if (NOT SDK_OPT_DESTINATION)
    set(SDK_OPT_DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  endif ()

  if (NOT SDK_OPT_SCRIPT)
    set(SDK_OPT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/syscfg.syscfg)
  endif ()

  if (NOT IS_ABSOLUTE ${SDK_OPT_SCRIPT})
    set(SDK_OPT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${SDK_OPT_SCRIPT})
  endif ()

  if (NOT EXISTS ${SDK_OPT_SCRIPT})
    message(FATAL_ERROR "sysconfig script \"${SDK_OPT_SCRIPT}\" not found")
  endif ()

  # Set SDK metadata file
  set(SDK_METADATA_FILE $ENV{simplelink_cc13xx_cc26xx_sdk_DIR}/.metadata/product.json)

  set(DST ${SDK_OPT_DESTINATION})

  add_library(${target} OBJECT ${DST}/ti_radio_config.c ${DST}/ti_drivers_config.c)

  add_custom_command(
    OUTPUT ${DST}/ti_radio_config.c ${DST}/ti_radio_config.h ${DST}/ti_drivers_config.c
           ${DST}/ti_drivers_config.h ${DST}/ti_utils_build_linker.cmd.genlibs
    COMMAND ${ti_sysconfig_bin} -s ${SDK_METADATA_FILE} --script ${SDK_OPT_SCRIPT} -o ${DST}
            --compiler gcc
    DEPENDS ${SDK_OPT_SCRIPT}
    COMMENT "Generating sysconfig.c from ${SDK_OPT_SCRIPT}"
    VERBATIM
  )

  target_include_directories(
    ${target}
    PUBLIC ${simplelink_cc13xx_cc26xx_sdk_DIR} ${simplelink_cc13xx_cc26xx_sdk_DIR}/source
           ${simplelink_cc13xx_cc26xx_sdk_DIR}/kernel/nortos
           ${simplelink_cc13xx_cc26xx_sdk_DIR}/kernel/nortos/posix ${DST}
  )

  target_link_directories(
    ${target} PUBLIC ${DST} ${simplelink_cc13xx_cc26xx_sdk_DIR}/kernel/nortos
    ${simplelink_cc13xx_cc26xx_sdk_DIR}/source
  )

  target_link_libraries(
    ${target}
    PUBLIC
      ${DST}/ti_utils_build_linker.cmd.genlibs
      ${simplelink_cc13xx_cc26xx_sdk_DIR}/source/ti/devices/cc13x2_cc26x2/driverlib/bin/gcc/driverlib.lib
  )
target_link_options(${target} PUBLIC --entry resetISR -nostartfiles)
endfunction ()
