# include several source files (in SRCS) in one or more
# combined files; each combined file holds maximally
# MAXNUM files.
# BASE gives basename for created files;
# files corresponding to the pattern are deleted
# first, so that there are no relicts from previous runs
# with possibly different MAXNUM
MACRO(COMBINE_SOURCES BASE SRCS MAXNUM COMBINED)
	FILE(GLOB EXISTING "${BASE}.*.cpp")
	MESSAGE(STATUS "Found existing ${EXISTING}")
	IF(EXISTING)
		MESSAGE(STATUS "Removing ${EXISTING}")
		FILE(REMOVE ${EXISTING})
	ENDIF()
	IF(${MAXNUM} LESS_EQUAL 1)
		set(${COMBINED} ${SRCS})
	ELSE()
		LIST(LENGTH SRCS SRCS_LENGTH)
		SET(COMB_COUNTER 0)
		SET(COUNTER ${MAXNUM})
		FOREACH(SRC ${SRCS})
			IF(${COUNTER} EQUAL ${MAXNUM})
				SET(COUNTER 0)
				SET(OUT "${BASE}.${COMB_COUNTER}.cpp")
				FILE(WRITE ${OUT})
				MESSAGE(STATUS "Creating ${OUT}")
				set(${COMBINED} ${${COMBINED}};${OUT})
				MATH(EXPR COMB_COUNTER "${COMB_COUNTER}+1")
			ENDIF()
			if(${SRC} MATCHES "^/.*$") # absolute filename
				FILE(APPEND ${OUT} "#include<${SRC}>\n")
			else()
				FILE(APPEND ${OUT} "#include<${CMAKE_SOURCE_DIR}/${SRC}>\n")
			endif()
			MATH(EXPR COUNTER "${COUNTER}+1")
		ENDFOREACH()
	ENDIF()
ENDMACRO()
