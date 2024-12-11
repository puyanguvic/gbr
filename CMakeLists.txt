check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/dgr-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME gbr
    SOURCE_FILES
        model/packet-tags.cc
        model/packet-headers.cc
        model/neighbor-status-database.cc
        model/udp-application.cc
        model/tcp-application.cc
        model/sink.cc
        model/routing-table-entry.cc
        model/ipv4-gbr-routing.cc
        model/router-interface.cc
        model/route-manager.cc
        model/route-manager-impl.cc
        model/route-candidate-queue.cc
        model/value-dense-queue-disc.cc
        helper/gbr-routing-helper.cc
        helper/tcp-application-helper.cc
        helper/sink-helper.cc
    HEADER_FILES
        model/packet-tags.h
        model/packet-headers.h
        model/neighbor-status-database.h
        model/udp-application.h
        model/tcp-application.h
        model/sink.h
        model/routing-table-entry.h
        model/ipv4-gbr-routing.h
        model/router-interface.h
        model/route-manager.h
        model/route-manager-impl.h
        model/route-candidate-queue.h
        model/value-dense-queue-disc.h
        helper/gbr-routing-helper.h
        helper/tcp-application-helper.h
        helper/sink-helper.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES
        # test/dgr-udp-application-test.cc
        # test/dgr-routing-table-test.cc
        # test/dgr-virtual-queue-test.cc
        # test/dgr-geant-topo-test.cc
        # test/dgr-header-test.cc
        # test/dgr-tcp-application-test.cc
        # 'test/dsr-tcp-applciation.cc
        # 'test/dsr-routing-test-suite.cc
        ${examples_as_tests_sources}
)
