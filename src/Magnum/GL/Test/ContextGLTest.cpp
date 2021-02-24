/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <algorithm>
#include <sstream>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/TestSuite/Compare/Numeric.h>

#include "Magnum/GL/Context.h"
#include "Magnum/GL/Extensions.h"
#include "Magnum/GL/OpenGLTester.h"
#include "Magnum/Platform/GLContext.h"

#ifndef CORRADE_TARGET_EMSCRIPTEN
#include <thread>
#endif

namespace Magnum { namespace GL { namespace Test { namespace {

struct ContextGLTest: OpenGLTester {
    explicit ContextGLTest();

    void stringFlags();

    void constructConfiguration();

    void makeCurrent();

    #ifndef CORRADE_TARGET_EMSCRIPTEN
    void multithreaded();
    #endif

    void isVersionSupported();
    #ifndef MAGNUM_TARGET_GLES
    void isVersionSupportedES();
    #endif
    void supportedVersion();
    void isExtensionSupported();
    void isExtensionDisabled();
};

using namespace Containers::Literals;

struct {
    const char* name;
    Containers::Optional<Extension> needsExtensionPresent, needsExtensionMissing;
    Context::Configuration::Flags flags;
    Containers::Array<Containers::StringView> disabledWorkarounds;
    Containers::Array<Extension> disabledExtensions;
    Containers::Array<const char*> args;
    Containers::StringView logShouldContain, logShouldNotContain;
} ConstructConfigurationData[] {
    {"default log", {}, {},
        {},
        {}, {}, {},
        "Renderer: ", {}},
    {"quiet", {}, {},
        Context::Configuration::Flag::QuietLog,
        {}, {}, {},
        {}, "Renderer: "},
    {"quiet on command line", {}, {}, {}, {}, {},
        Containers::array({"", "--magnum-log", "quiet"}),
        {}, "Renderer: "},
    {"quiet and verbose", {}, {},
        Context::Configuration::Flag::QuietLog|Context::Configuration::Flag::VerboseLog,
        {}, {}, {},
        /* Verbose has a precedence */
        "Renderer: ", {}},
    {"quiet and verbose on command line", {}, {},
        Context::Configuration::Flag::QuietLog,
        {}, {},
        Containers::array({"", "--magnum-log", "verbose"}),
        /* Command-line has a precedence */
        "Renderer: ", {}},
    {"verbose and quiet on command line", {}, {},
        Context::Configuration::Flag::VerboseLog,
        {}, {},
        Containers::array({"", "--magnum-log", "quiet"}),
        /* Command-line has a precedence */
        {}, "Renderer: "},
    #ifndef MAGNUM_TARGET_GLES
    {"default workarounds", {}, {}, {}, {}, {}, {},
        "\nUsing driver workarounds:\n    no-layout-qualifiers-on-old-glsl\n", {}},
    {"disabled workaround", {}, {}, {},
        Containers::array({"no-layout-qualifiers-on-old-glsl"_s}), {}, {},
        {}, "no-layout-qualifiers-on-old-glsl"},
    {"disabled workaround on command line", {}, {}, {}, {}, {},
        Containers::array({"", "--magnum-disable-workarounds", "no-layout-qualifiers-on-old-glsl"}),
        {}, "no-layout-qualifiers-on-old-glsl"},
    #endif
    #ifndef MAGNUM_TARGET_GLES
    {"default extensions ARB",
        Extension{Extensions::ARB::texture_filter_anisotropic{}},
        {}, {}, {}, {}, {},
        "    GL_ARB_texture_filter_anisotropic\n", {}},
    #endif
    {"default extensions EXT",
        Extension{Extensions::EXT::texture_filter_anisotropic{}},
        #ifndef MAGNUM_TARGET_GLES
        Extension{Extensions::ARB::texture_filter_anisotropic{}},
        #else
        {},
        #endif
        {}, {}, {}, {},
        "    GL_EXT_texture_filter_anisotropic\n", {}},
    {"disabled extension",
        Extension{Extensions::EXT::texture_filter_anisotropic{}},
        {}, {}, {},
        Containers::array<Extension>({Extensions::EXT::texture_filter_anisotropic{}}), {},
        "Disabling extensions:\n    GL_EXT_texture_filter_anisotropic\n", {}},
    {"disabled extension on command line",
        Extension{Extensions::EXT::texture_filter_anisotropic{}},
        {}, {}, {}, {},
        Containers::array({"", "--magnum-disable-extensions", "GL_EXT_texture_filter_anisotropic"}),
        "Disabling extensions:\n    GL_EXT_texture_filter_anisotropic\n", {}},
};

ContextGLTest::ContextGLTest() {
    addTests({&ContextGLTest::stringFlags});

    addInstancedTests({&ContextGLTest::constructConfiguration},
        Containers::arraySize(ConstructConfigurationData));

    addTests({
        &ContextGLTest::makeCurrent,

        #ifndef CORRADE_TARGET_EMSCRIPTEN
        &ContextGLTest::multithreaded,
        #endif

        &ContextGLTest::isVersionSupported,
        #ifndef MAGNUM_TARGET_GLES
        &ContextGLTest::isVersionSupportedES,
        #endif
        &ContextGLTest::supportedVersion,
        &ContextGLTest::isExtensionSupported,
        &ContextGLTest::isExtensionDisabled});
}

void ContextGLTest::stringFlags() {
    Context& context = Context::current();

    CORRADE_VERIFY(!context.vendorString().isEmpty());
    CORRADE_COMPARE(context.vendorString().flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);

    CORRADE_VERIFY(!context.rendererString().isEmpty());
    CORRADE_COMPARE(context.rendererString().flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);

    CORRADE_VERIFY(!context.versionString().isEmpty());
    CORRADE_COMPARE(context.versionString().flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);

    CORRADE_VERIFY(!context.shadingLanguageVersionString().isEmpty());
    CORRADE_COMPARE(context.shadingLanguageVersionString().flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);

    for(Containers::StringView languageVersion: context.shadingLanguageVersionStrings()) {
        /* One of these might be empty */
        CORRADE_COMPARE(languageVersion.flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);
    }

    for(Containers::StringView extension: context.extensionStrings()) {
        CORRADE_VERIFY(!extension.isEmpty());

        /* On GL 2.1 and GLES2 the extensions are split from a long string and
           thus aren't all null-terminated, only the last one */
        #ifndef MAGNUM_TARGET_GLES
        if(context.isVersionSupported(Version::GL300))
        #else
        if(context.isVersionSupported(Version::GLES300))
        #endif
        {
            CORRADE_COMPARE(extension.flags(), Containers::StringViewFlag::Global|Containers::StringViewFlag::NullTerminated);
        } else {
            CORRADE_COMPARE_AS(extension.flags(), Containers::StringViewFlag::Global,
                TestSuite::Compare::GreaterOrEqual);
        }
    }
}

void ContextGLTest::constructConfiguration() {
    auto&& data = ConstructConfigurationData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(std::getenv("MAGNUM_DISABLE_WORKAROUNDS"))
        CORRADE_SKIP("Can't test with the MAGNUM_DISABLE_WORKAROUNDS environment variable set");
    if(std::getenv("MAGNUM_DISABLE_EXTENSIONS"))
        CORRADE_SKIP("Can't test with the MAGNUM_DISABLE_EXTENSIONS environment variable set");

    CORRADE_VERIFY(Context::hasCurrent());

    if(data.needsExtensionPresent && !Context::current().isExtensionSupported(*data.needsExtensionPresent))
        CORRADE_SKIP(data.needsExtensionPresent->string() + std::string{" is not supported, skippping"});
    if(data.needsExtensionMissing && Context::current().isExtensionSupported(*data.needsExtensionMissing))
        CORRADE_SKIP(data.needsExtensionMissing->string() + std::string{" is supported, skippping"});

    std::ostringstream out;
    {
        Context* current = &Context::current();
        Context::makeCurrent(nullptr);
        Containers::ScopeGuard resetCurrent{current, Context::makeCurrent};

        Debug redirectOut{&out};
        Platform::GLContext ctx{Int(data.args.size()), data.args, Context::Configuration{}
            .setFlags(data.flags)
            .addDisabledWorkarounds(data.disabledWorkarounds)
            .addDisabledExtensions(data.disabledExtensions)
        };
    }

    /** @todo TestSuite::Compare::StringContains / NotContains for proper diag */
    if(!data.logShouldContain.isEmpty())
        CORRADE_VERIFY(Containers::StringView{out.str()}.contains(data.logShouldContain));
    if(!data.logShouldNotContain.isEmpty())
        CORRADE_VERIFY(!Containers::StringView{out.str()}.contains(data.logShouldNotContain));
}

void ContextGLTest::makeCurrent() {
    CORRADE_VERIFY(Context::hasCurrent());

    Context& current = Context::current();
    Context::makeCurrent(nullptr);

    CORRADE_VERIFY(!Context::hasCurrent());

    {
        Platform::GLContext ctx{Context::Configuration{}
            .setFlags(Context::Configuration::Flag::QuietLog)
        };

        CORRADE_VERIFY(Context::hasCurrent());
    }

    CORRADE_VERIFY(!Context::hasCurrent());

    Context::makeCurrent(&current);

    CORRADE_VERIFY(Context::hasCurrent());
    CORRADE_COMPARE(&Context::current(), &current);
}

#ifndef CORRADE_TARGET_EMSCRIPTEN
void ContextGLTest::multithreaded() {
    CORRADE_VERIFY(Context::hasCurrent());
    Containers::Optional<bool> otherThreadHasCurrent;

    std::thread t{[](Containers::Optional<bool>& hasCurrent) {
        hasCurrent = Context::hasCurrent();
    }, std::ref(otherThreadHasCurrent)};

    t.join();

    CORRADE_VERIFY(otherThreadHasCurrent);

    Debug{} << "CORRADE_BUILD_MULTITHREADED defined:" <<
        #ifdef CORRADE_BUILD_MULTITHREADED
        true
        #else
        false
        #endif
        ;

    Debug{} << "Current context visible in another thread:" << *otherThreadHasCurrent;

    #ifdef CORRADE_BUILD_MULTITHREADED
    CORRADE_VERIFY(!*otherThreadHasCurrent);
    #else
    CORRADE_VERIFY(*otherThreadHasCurrent);
    #endif
}
#endif

void ContextGLTest::isVersionSupported() {
    const Version v = Context::current().version();
    CORRADE_VERIFY(Context::current().isVersionSupported(v));
    CORRADE_VERIFY(Context::current().isVersionSupported(Version(Int(v)-1)));
    CORRADE_VERIFY(!Context::current().isVersionSupported(Version(Int(v)+1)));

    /* No assertions should be fired */
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(v);
    MAGNUM_ASSERT_GL_VERSION_SUPPORTED(Version(Int(v)-1));
}

#ifndef MAGNUM_TARGET_GLES
void ContextGLTest::isVersionSupportedES() {
    if(!Context::current().isExtensionSupported<Extensions::ARB::ES2_compatibility>())
        CORRADE_SKIP(Extensions::ARB::ES2_compatibility::string() + std::string(" extension should not be supported, can't test"));

    /* No assertions should be fired */
    CORRADE_VERIFY(Context::current().isVersionSupported(Version::GLES200));
}
#endif

void ContextGLTest::supportedVersion() {
    const Version v = Context::current().version();

    /* Selects first supported version (thus not necessarily the highest) */
    CORRADE_VERIFY(Context::current().supportedVersion({Version(Int(v)+1), v, Version(Int(v)-1)}) == v);
    CORRADE_VERIFY(Context::current().supportedVersion({Version(Int(v)+1), Version(Int(v)-1), v}) == Version(Int(v)-1));
}

void ContextGLTest::isExtensionSupported() {
    #ifndef MAGNUM_TARGET_GLES
    if(Context::current().isExtensionSupported<Extensions::GREMEDY::string_marker>())
        CORRADE_SKIP(Extensions::GREMEDY::string_marker::string() + std::string(" extension should not be supported, can't test"));

    if(!Context::current().isExtensionSupported<Extensions::EXT::texture_filter_anisotropic>())
        CORRADE_SKIP(Extensions::EXT::texture_filter_anisotropic::string() + std::string(" extension should be supported, can't test"));

    if(!Context::current().isExtensionSupported<Extensions::ARB::explicit_attrib_location>())
        CORRADE_SKIP(Extensions::ARB::explicit_attrib_location::string() + std::string(" extension should be supported, can't test"));

    /* Test that we have proper extension list parser */
    Containers::Array<Containers::StringView> extensions = Context::current().extensionStrings();
    CORRADE_VERIFY(std::find(extensions.begin(), extensions.end(),
        Extensions::EXT::texture_filter_anisotropic::string()) != extensions.end());
    CORRADE_VERIFY(std::find(extensions.begin(), extensions.end(),
        Extensions::GREMEDY::string_marker::string()) == extensions.end());

    /* This is disabled in GL < 3.2 to work around GLSL compiler bugs */
    CORRADE_VERIFY(!Context::current().isExtensionSupported<Extensions::ARB::explicit_attrib_location>(Version::GL310));
    CORRADE_VERIFY(Context::current().isExtensionSupported<Extensions::ARB::explicit_attrib_location>(Version::GL320));
    #else
    CORRADE_SKIP("No useful extensions to test on OpenGL ES");
    #endif
}

void ContextGLTest::isExtensionDisabled() {
    #ifndef MAGNUM_TARGET_GLES
    if(!Context::current().isExtensionSupported<Extensions::ARB::vertex_array_object>())
        CORRADE_SKIP(Extensions::ARB::vertex_array_object::string() + std::string(" extension should be supported, can't test"));

    if(!Context::current().isExtensionSupported<Extensions::ARB::explicit_attrib_location>())
        CORRADE_SKIP(Extensions::ARB::explicit_attrib_location::string() + std::string(" extension should be supported, can't test"));

    /* This is not disabled anywhere */
    CORRADE_VERIFY(!Context::current().isExtensionDisabled<Extensions::ARB::vertex_array_object>());

    /* This is disabled in GL < 3.2 to work around GLSL compiler bugs */
    CORRADE_VERIFY(Context::current().isExtensionDisabled<Extensions::ARB::explicit_attrib_location>(Version::GL310));
    CORRADE_VERIFY(!Context::current().isExtensionDisabled<Extensions::ARB::explicit_attrib_location>(Version::GL320));
    #else
    CORRADE_SKIP("No useful extensions to test on OpenGL ES");
    #endif
}

}}}}

CORRADE_TEST_MAIN(Magnum::GL::Test::ContextGLTest)
