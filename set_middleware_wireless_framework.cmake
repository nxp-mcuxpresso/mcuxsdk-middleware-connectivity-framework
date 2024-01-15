include_guard(GLOBAL)


if (CONFIG_USE_middleware_wireless_framework_sec_lib)
# Add set(CONFIG_USE_middleware_wireless_framework_sec_lib true) in config.cmake to use this component

message("middleware_wireless_framework_sec_lib component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/./SecLib/SecLib.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/./SecLib
)

if((CONFIG_TOOLCHAIN STREQUAL armgcc OR CONFIG_TOOLCHAIN STREQUAL mcux))
  target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE
    -Wl,--start-group
      ${CMAKE_CURRENT_LIST_DIR}/./SecLib/lib_crypto_m7.a
      -Wl,--end-group
  )
endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_common_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_common_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_common_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_coex_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_coex_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_coex_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_board_lp_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_board_dcdc_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_board_platform_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_board_comp_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_board_extflash_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_lfs_config_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_lfs_config_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_lfs_config_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/configs
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DLFS_CONFIG=fwk_lfs_config.h
  )

endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_matter_config_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MIMXRT1062/gcc
)


endif()


if (CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_init_config_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1062
)


endif()


if (CONFIG_USE_middleware_wireless_framework_rpmsg_config)
# Add set(CONFIG_USE_middleware_wireless_framework_rpmsg_config true) in config.cmake to use this component

message("middleware_wireless_framework_rpmsg_config component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/configs
)


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_rt_ot_coex)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_rt_ot_coex true) in config.cmake to use this component

message("middleware_wireless_framework_platform_rt_ot_coex component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/fwk_platform_ot.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/fwk_platform_hdlc.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/k32w0/fwk_platform_coex.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/iw612/fwk_platform_coex.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/configs/fwk_lfs_config.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/imx_rt/configs
)


endif()


if (CONFIG_USE_middleware_wireless_framework_CMake_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_CMake_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_CMake_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")


endif()


if (CONFIG_USE_middleware_wireless_framework_FSCI)
# Add set(CONFIG_USE_middleware_wireless_framework_FSCI true) in config.cmake to use this component

message("middleware_wireless_framework_FSCI component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Source/FsciCommands.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Source/FsciCommunication.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Source/FsciLogging.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Source/FsciMain.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Interface
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSCI/Source
)


endif()


if (CONFIG_USE_middleware_wireless_HWParameter)
# Add set(CONFIG_USE_middleware_wireless_HWParameter true) in config.cmake to use this component

message("middleware_wireless_HWParameter component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/HWParameter/HWParameter.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/HWParameter
)


endif()


if (CONFIG_USE_middleware_wireless_framework_Common)
# Add set(CONFIG_USE_middleware_wireless_framework_Common true) in config.cmake to use this component

message("middleware_wireless_framework_Common component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common
)


endif()


if (CONFIG_USE_middleware_wireless_framework_RNG)
# Add set(CONFIG_USE_middleware_wireless_framework_RNG true) in config.cmake to use this component

message("middleware_wireless_framework_RNG component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/RNG/RNG.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/RNG
)


endif()


if (CONFIG_USE_middleware_wireless_framework_RNG_mbedtls)
# Add set(CONFIG_USE_middleware_wireless_framework_RNG_mbedtls true) in config.cmake to use this component

message("middleware_wireless_framework_RNG_mbedtls component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/RNG/RNG_mbedTLS.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/RNG
)


endif()


if (CONFIG_USE_middleware_wireless_framework_sec_lib_cryptolib_src)
# Add set(CONFIG_USE_middleware_wireless_framework_sec_lib_cryptolib_src true) in config.cmake to use this component

message("middleware_wireless_framework_sec_lib_cryptolib_src component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/SecLib.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/build/CryptoLib/src/SW_AES128.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/build/CryptoLib/src/SW_RNG.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/build/CryptoLib/src/SW_SHA1.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/build/CryptoLib/src/SW_SHA256.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/build/CryptoLib/src
)


endif()


if (CONFIG_USE_middleware_wireless_framework_function_lib)
# Add set(CONFIG_USE_middleware_wireless_framework_function_lib true) in config.cmake to use this component

message("middleware_wireless_framework_function_lib component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FunctionLib/FunctionLib.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FunctionLib
)


endif()


if (CONFIG_USE_middleware_wireless_framework_module_info)
# Add set(CONFIG_USE_middleware_wireless_framework_module_info true) in config.cmake to use this component

message("middleware_wireless_framework_module_info component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/ModuleInfo/ModuleInfo.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/ModuleInfo
)


endif()


if (CONFIG_USE_middleware_wireless_framework_NVM)
# Add set(CONFIG_USE_middleware_wireless_framework_NVM true) in config.cmake to use this component

message("middleware_wireless_framework_NVM component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/NVM/Source/NV_Flash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/NVM/Interface
  ${CMAKE_CURRENT_LIST_DIR}/../framework/NVM/Source
)


endif()


if (CONFIG_USE_middleware_wireless_framework_NV_FSCI)
# Add set(CONFIG_USE_middleware_wireless_framework_NV_FSCI true) in config.cmake to use this component

message("middleware_wireless_framework_NV_FSCI component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/NVM/Source/NV_FsciCommands.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/NVM/Source
)


endif()


if (CONFIG_USE_middleware_wireless_framework_OtaServerSupport)
# Add set(CONFIG_USE_middleware_wireless_framework_OtaServerSupport true) in config.cmake to use this component

message("middleware_wireless_framework_OtaServerSupport component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source/OtaServerSupport.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Interface
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source
)


endif()


if (CONFIG_USE_middleware_wireless_framework_sbtsnoop_ethermind_port)
# Add set(CONFIG_USE_middleware_wireless_framework_sbtsnoop_ethermind_port true) in config.cmake to use this component

message("middleware_wireless_framework_sbtsnoop_ethermind_port component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/DBG/sbtsnoop
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DSBTSNOOP_PORT_FILE=sbtsnoop_port_ethermind.h
    -DSERIAL_BTSNOOP
  )

endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_sbtsnoop_nxp_ble_port)
# Add set(CONFIG_USE_middleware_wireless_framework_sbtsnoop_nxp_ble_port true) in config.cmake to use this component

message("middleware_wireless_framework_sbtsnoop_nxp_ble_port component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/DBG/sbtsnoop
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DSBTSNOOP_PORT_FILE=sbtsnoop_port_nxp_ble.h
    -DTM_ENABLE_TIME_STAMP=1
    -DSERIAL_BTSNOOP
  )

endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_fwk_debug)
# Add set(CONFIG_USE_middleware_wireless_framework_fwk_debug true) in config.cmake to use this component

message("middleware_wireless_framework_fwk_debug component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/DBG
)


endif()


if (CONFIG_USE_middleware_wireless_framework_linkscripts_kw45)
# Add set(CONFIG_USE_middleware_wireless_framework_linkscripts_kw45 true) in config.cmake to use this component

message("middleware_wireless_framework_linkscripts_kw45 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_TOOLCHAIN STREQUAL mcux)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript/end_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript/main_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript/symbols.ldt
  )
endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_linkscripts_mcxw34xevk)
# Add set(CONFIG_USE_middleware_wireless_framework_linkscripts_mcxw34xevk true) in config.cmake to use this component

message("middleware_wireless_framework_linkscripts_mcxw34xevk component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_TOOLCHAIN STREQUAL mcux)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MCXW345/mcux/linkscript/end_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MCXW345/mcux/linkscript/main_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MCXW345/mcux/linkscript/symbols.ldt
  )
endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_linkscript_bootloader_kw45)
# Add set(CONFIG_USE_middleware_wireless_framework_linkscript_bootloader_kw45 true) in config.cmake to use this component

message("middleware_wireless_framework_linkscript_bootloader_kw45 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_TOOLCHAIN STREQUAL mcux)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_bootloader/end_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_bootloader/main_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_bootloader/main_text_section.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_bootloader/symbols.ldt
  )
endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_linkscript_warmboot_kw45)
# Add set(CONFIG_USE_middleware_wireless_framework_linkscript_warmboot_kw45 true) in config.cmake to use this component

message("middleware_wireless_framework_linkscript_warmboot_kw45 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_TOOLCHAIN STREQUAL mcux)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_warmboot/end_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_warmboot/main_text.ldt
      ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/kw45_k32w1/mcux/linkscript_warmboot/symbols.ldt
  )
endif()


endif()


if (CONFIG_USE_middleware_wireless_HDI)
# Add set(CONFIG_USE_middleware_wireless_HDI true) in config.cmake to use this component

message("middleware_wireless_HDI component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/HDI/hdi.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/HDI
)


endif()


if (CONFIG_USE_middleware_wireless_IPC)
# Add set(CONFIG_USE_middleware_wireless_IPC true) in config.cmake to use this component

message("middleware_wireless_IPC component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/IPC/ipc.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/IPC
)


endif()


if (CONFIG_USE_middleware_wireless_framework_PDM)
# Add set(CONFIG_USE_middleware_wireless_framework_PDM true) in config.cmake to use this component

message("middleware_wireless_framework_PDM component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/PDM/Include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_PDUM)
# Add set(CONFIG_USE_middleware_wireless_framework_PDUM true) in config.cmake to use this component

message("middleware_wireless_framework_PDUM component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/PDUM/Include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_markdown)
# Add set(CONFIG_USE_middleware_wireless_framework_markdown true) in config.cmake to use this component

message("middleware_wireless_framework_markdown component is included from ${CMAKE_CURRENT_LIST_FILE}.")


endif()


if (CONFIG_USE_middleware_wireless_framework_SFC)
# Add set(CONFIG_USE_middleware_wireless_framework_SFC true) in config.cmake to use this component

message("middleware_wireless_framework_SFC component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SFC
)


endif()


if (CONFIG_USE_middleware_wireless_framework_RF_SFC)
# Add set(CONFIG_USE_middleware_wireless_framework_RF_SFC true) in config.cmake to use this component

message("middleware_wireless_framework_RF_SFC component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SFC/fwk_rf_sfc.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SFC
)


endif()


if (CONFIG_USE_middleware_wireless_framework_OTW)
# Add set(CONFIG_USE_middleware_wireless_framework_OTW true) in config.cmake to use this component

message("middleware_wireless_framework_OTW component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OTW/k32w0_transceiver/fwk_otw.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OTW/Interface
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OTW/k32w0_transceiver
)


endif()


if (CONFIG_USE_middleware_wireless_framework_FactoryDataProvider)
# Add set(CONFIG_USE_middleware_wireless_framework_FactoryDataProvider true) in config.cmake to use this component

message("middleware_wireless_framework_FactoryDataProvider component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FactoryDataProvider/fwk_factory_data_provider.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FactoryDataProvider
)


endif()


if (CONFIG_USE_middleware_wireless_framework_filesystem)
# Add set(CONFIG_USE_middleware_wireless_framework_filesystem true) in config.cmake to use this component

message("middleware_wireless_framework_filesystem component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FileSystem/fwk_filesystem.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FileSystem
)


endif()


if (CONFIG_USE_middleware_wireless_framework_fsabstraction_littlefs)
# Add set(CONFIG_USE_middleware_wireless_framework_fsabstraction_littlefs true) in config.cmake to use this component

message("middleware_wireless_framework_fsabstraction_littlefs component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSAbstraction/fwk_lfs_mflash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSAbstraction
)


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_common_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_common_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_common_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_coex_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_coex_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_coex_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_board_lp_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_board_dcdc_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_board_platform_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_board_comp_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_board_extflash_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_lfs_config_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_lfs_config_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_lfs_config_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/configs
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DLFS_CONFIG=fwk_lfs_config.h
  )

endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_matter_config_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MIMXRT1176/gcc
)


endif()


if (CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_init_config_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT1176
)


endif()


if (CONFIG_USE_middleware_wireless_framework_CMake_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_CMake_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_CMake_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_common_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_common_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_common_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_coex_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_coex_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_coex_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_board_lp_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_board_lp_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S/board_lp.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_board_dcdc_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_board_dcdc_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_board_platform_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_board_platform_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_board_comp_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_board_comp_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_board_extflash_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_board_extflash_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_lfs_config_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_lfs_config_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_lfs_config_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S/configs
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DLFS_CONFIG=fwk_lfs_config.h
  )

endif()


endif()


if (CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_matter_config_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_matter_config_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S/board_lp.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/devices/MIMXRT595S/gcc
)


endif()


if (CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_init_config_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_init_config_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S/app_services_init.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/boards/MIMXRT595S
)


endif()


if (CONFIG_USE_middleware_wireless_framework_CMake_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_CMake_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_CMake_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")


endif()


if (CONFIG_USE_middleware_wireless_framework_platform_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_fwk_debug)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/fwk_platform.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/configs
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_flash_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_flash_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_flash_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/fwk_platform_flash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_flash_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_extflash_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_extflash_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_extflash_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_extflash_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_sensors_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_sensors_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_sensors_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_sensors_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ota_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ota_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ota_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060/fwk_platform_ota.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ota_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lcl_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lcl_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lcl_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_driver_trdc AND CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lcl_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_mws_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_mws_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_mws_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_mws_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ble_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ble_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ble_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060 AND CONFIG_USE_middleware_wireless_framework_platform_coex_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ble_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_genfsk_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_genfsk_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_genfsk_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_genfsk_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ot_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ot_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ot_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ot_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1060)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1060 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_timer_rt1060 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1060
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_timer_rt1060 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_sec_lib_sss_m7)
# Add set(CONFIG_USE_middleware_wireless_framework_sec_lib_sss_m7 true) in config.cmake to use this component

message("middleware_wireless_framework_sec_lib_sss_m7 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/SecLib_sss.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib
)

if((CONFIG_TOOLCHAIN STREQUAL armgcc OR CONFIG_TOOLCHAIN STREQUAL mcux))
  target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE
    -Wl,--start-group
      ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/lib_crypto_m7.a
      -Wl,--end-group
  )
endif()

else()

message(SEND_ERROR "middleware_wireless_framework_sec_lib_sss_m7 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_OtaSupport)
# Add set(CONFIG_USE_middleware_wireless_framework_OtaSupport true) in config.cmake to use this component

message("middleware_wireless_framework_OtaSupport component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_ota_rt1060 AND CONFIG_USE_middleware_wireless_framework_platform_extflash_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source/OtaSupport.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source/OtaInternalFlash.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source/OtaExternalFlash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Interface
  ${CMAKE_CURRENT_LIST_DIR}/../framework/OtaSupport/Source
)

else()

message(SEND_ERROR "middleware_wireless_framework_OtaSupport dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_sbtsnoop)
# Add set(CONFIG_USE_middleware_wireless_framework_sbtsnoop true) in config.cmake to use this component

message("middleware_wireless_framework_sbtsnoop component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_sbtsnoop_ethermind_port OR CONFIG_USE_middleware_wireless_framework_sbtsnoop_nxp_ble_port)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/DBG/sbtsnoop/sbtsnoop.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/DBG/sbtsnoop
)

else()

message(SEND_ERROR "middleware_wireless_framework_sbtsnoop dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_freertos_heap)
# Add set(CONFIG_USE_middleware_wireless_freertos_heap true) in config.cmake to use this component

message("middleware_wireless_freertos_heap component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_freertos-kernel)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/rtos/freertos/heap_mem_manager.c
)

else()

message(SEND_ERROR "middleware_wireless_freertos_heap dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_freertos_utils)
# Add set(CONFIG_USE_middleware_wireless_framework_freertos_utils true) in config.cmake to use this component

message("middleware_wireless_framework_freertos_utils component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_freertos-kernel AND CONFIG_USE_middleware_wireless_framework_platform_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/rtos/freertos/fwk_freertos_utils.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/Common/rtos/freertos
)

else()

message(SEND_ERROR "middleware_wireless_framework_freertos_utils dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1060 AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_MIMXRT1062 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_systicks_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1060 AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1060)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

if(CONFIG_USE_middleware_freertos-kernel)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks.c
  )
endif()

if(CONFIG_USE_middleware_baremetal)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks_bm.c
  )
endif()

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_systicks_MIMXRT1062 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT1062)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT1062 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_cli_MIMXRT1062 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1062)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_cli.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_cli_MIMXRT1062 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_fsabstraction)
# Add set(CONFIG_USE_middleware_wireless_framework_fsabstraction true) in config.cmake to use this component

message("middleware_wireless_framework_fsabstraction component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_fsabstraction_littlefs)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSAbstraction/fwk_fs_abstraction.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FSAbstraction
)

else()

message(SEND_ERROR "middleware_wireless_framework_fsabstraction dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_filecache)
# Add set(CONFIG_USE_middleware_wireless_framework_filecache true) in config.cmake to use this component

message("middleware_wireless_framework_filecache component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_fsabstraction)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FileCache/fwk_file_cache.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/FileCache
)

else()

message(SEND_ERROR "middleware_wireless_framework_filecache dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_keystorage)
# Add set(CONFIG_USE_middleware_wireless_framework_keystorage true) in config.cmake to use this component

message("middleware_wireless_framework_keystorage component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_filecache)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/KeyStorage/fwk_key_storage.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/KeyStorage
)

else()

message(SEND_ERROR "middleware_wireless_framework_keystorage dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_fwk_debug)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/fwk_platform.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/configs
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_flash_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_flash_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_flash_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/fwk_platform_flash.c
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/fwk_platform_extflash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_flash_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_extflash_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_extflash_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_extflash_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/fwk_platform_extflash.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_extflash_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_sensors_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_sensors_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_sensors_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_sensors_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ota_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ota_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ota_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170/fwk_platform_ota.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ota_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lcl_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lcl_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lcl_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_driver_trdc AND CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lcl_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_mws_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_mws_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_mws_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_mws_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ble_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ble_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ble_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170 AND CONFIG_USE_middleware_wireless_framework_platform_coex_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ble_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_genfsk_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_genfsk_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_genfsk_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_genfsk_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ot_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ot_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ot_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ot_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1170)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1170 true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_timer_rt1170 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_rt1170)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/rt1170
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_timer_rt1170 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1170 AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1170)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_MIMXRT1176 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_systicks_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_rt1170 AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_rt1170)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

if(CONFIG_USE_middleware_freertos-kernel)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks.c
  )
endif()

if(CONFIG_USE_middleware_baremetal)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks_bm.c
  )
endif()

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_systicks_MIMXRT1176 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT1176)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT1176 true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_cli_MIMXRT1176 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT1176)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_cli.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_cli_MIMXRT1176 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_fwk_debug)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S/fwk_platform.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/include
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S/configs
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_flash_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_flash_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_flash_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_flash_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_extflash_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_extflash_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_extflash_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/Common
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_extflash_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_sensors_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_sensors_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_sensors_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_sensors_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ota_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ota_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ota_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ota_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lcl_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lcl_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lcl_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_driver_trdc AND CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lcl_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_mws_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_mws_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_mws_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_mws_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ble_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ble_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ble_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S AND CONFIG_USE_middleware_wireless_framework_platform_coex_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ble_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_genfsk_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_genfsk_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_genfsk_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_genfsk_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_ot_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_ot_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_ot_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_ot_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S/fwk_platform_lowpower.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/platform/MIMXRT595S
)

else()

message(SEND_ERROR "middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_sec_lib_sss_m33)
# Add set(CONFIG_USE_middleware_wireless_framework_sec_lib_sss_m33 true) in config.cmake to use this component

message("middleware_wireless_framework_sec_lib_sss_m33 component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_MIMXRT595S)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/SecLib_sss.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib
)

if((CONFIG_TOOLCHAIN STREQUAL armgcc OR CONFIG_TOOLCHAIN STREQUAL mcux))
  target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE
    -Wl,--start-group
      ${CMAKE_CURRENT_LIST_DIR}/../framework/SecLib/lib_crypto_m33.a
      -Wl,--end-group
  )
endif()

else()

message(SEND_ERROR "middleware_wireless_framework_sec_lib_sss_m33 dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_MIMXRT595S AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_systicks_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_systicks_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_platform_lowpower_MIMXRT595S AND CONFIG_USE_middleware_wireless_framework_platform_lowpower_timer_MIMXRT595S)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR.c
)

if(CONFIG_USE_middleware_freertos-kernel)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks.c
  )
endif()

if(CONFIG_USE_middleware_baremetal)
  target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_systicks_bm.c
  )
endif()

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

if(CONFIG_USE_COMPONENT_CONFIGURATION)
  message("===>Import configuration from ${CMAKE_CURRENT_LIST_FILE}")

  target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PUBLIC
    -DHAL_UART_ADAPTER_LOWPOWER=1
  )

endif()

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_systicks_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()


if (CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT595S)
# Add set(CONFIG_USE_middleware_wireless_framework_LPM_cli_MIMXRT595S true) in config.cmake to use this component

message("middleware_wireless_framework_LPM_cli_MIMXRT595S component is included from ${CMAKE_CURRENT_LIST_FILE}.")

if(CONFIG_USE_middleware_wireless_framework_LPM_MIMXRT595S)

target_sources(${MCUX_SDK_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower/PWR_cli.c
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../framework/LowPower
)

else()

message(SEND_ERROR "middleware_wireless_framework_LPM_cli_MIMXRT595S dependency does not meet, please check ${CMAKE_CURRENT_LIST_FILE}.")

endif()

endif()

