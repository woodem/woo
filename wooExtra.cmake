message(STATUS "Installing @CMAKE_SOURCE_DIR@/wooExtra/*")
file(GLOB WOOEXTRA_SETUPS @CMAKE_SOURCE_DIR@/wooExtra/*/setup.py)
foreach(SETUP ${WOOEXTRA_SETUPS})
	get_filename_component(SETUP_DIR ${SETUP} DIRECTORY)
	# call with --user only if not in a virtual environment (test env var VIRTUAL_ENV)
	if(NOT DEFINED ENV{VIRTUAL_ENV})
		set(SETUPPY_EXTRA "--user")
	endif()
	message(STATUS "in ${SETUP_DIR}: @PYTHON_EXECUTABLE@ -W ignore '${SETUP}' --quiet install ${SETUPPY_EXTRA}")
	execute_process(
		COMMAND @PYTHON_EXECUTABLE@ -W ignore "${SETUP}" --quiet install ${SETUPPY_EXTRA}
		WORKING_DIRECTORY ${SETUP_DIR}
		RESULT_VARIABLE SETUP_STATUS
		OUTPUT_VARIABLE SETUP_OUTPUT
		ERROR_VARIABLE SETUP_OUTPUT
	)
	if(NOT "${SETUP_STATUS}" EQUAL 0)
		message(FATAL_ERROR "Error installing from ${SETUP} (${SETUP_STATUS}):\n${SETUP_OUTPUT}")
	endif()
endforeach()

