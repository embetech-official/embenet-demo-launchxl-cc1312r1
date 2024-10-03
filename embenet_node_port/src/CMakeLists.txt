add_library(
  embenet_node_port_cc1312 STATIC
  embenet_aes128.c
  embenet_brt.c
  embenet_capabilities.c
  embenet_eui64.c
  embenet_critical_section.c
  embenet_radio.c
  embenet_random.c
  embenet_timer.c
)

include(FetchContent)

FetchContent_Declare(
  embenet_node_port_interface
  SVN_REPOSITORY https://svn.embe.tech/embenet_src/libs/embenet_node_port_interface/trunk/
  FIND_PACKAGE_ARGS NAMES embetech::embenet_node_port_interface
)

FetchContent_MakeAvailable(embenet_node_port_interface)

simplelink_sdk_target(cc1312_sdk SCRIPT embenet_node_port.syscfg)
target_compile_definitions(cc1312_sdk PUBLIC DeviceFamily_CC13X2)

target_link_libraries(embenet_node_port_cc1312 PUBLIC embenet_node_port_interface cc1312_sdk)
