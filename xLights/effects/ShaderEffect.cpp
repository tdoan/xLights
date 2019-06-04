#include "../../include/shader_64.xpm"
#include "../../include/shader_48.xpm"
#include "../../include/shader_32.xpm"
#include "../../include/shader_24.xpm"
#include "../../include/shader_16.xpm"

#include "ShaderEffect.h"
#include "ShaderPanel.h"
#include "../sequencer/Effect.h"
#include "../RenderBuffer.h"
#include "../UtilClasses.h"
#include "../xLightsMain.h"
#include "../xLightsApp.h"
#include "../TimingPanel.h"
#include "OpenGLShaders.h"
#include "UtilFunctions.h"
#include "../../xSchedule/wxJSON/jsonreader.h"

#include <wx/regex.h>

// Ack... forgot the old warp effect needed all of this!!
#ifndef __WXMAC__
#include <GL/gl.h>
#ifdef _MSC_VER
#include "GL\glext.h"
#else
#include <GL/glext.h>
#endif

extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix4fv;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM4FPROC glUniform4f;
#else
#include "OpenGL/gl.h"
#endif

#include <log4cpp/Category.hh>
#include <fstream>

namespace
{
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

    GLuint RenderBufferTexture(int w, int h)
    {
        GLuint texId = 0;

        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        return texId;
    }

    bool createOpenGLRenderBuffer(int width, int height, GLuint* rbID, GLuint* fbID)
    {
        glGenRenderbuffers(1, rbID);
        glBindRenderbuffer(GL_RENDERBUFFER, *rbID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, width, height);

        glGenFramebuffers(1, fbID);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbID);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *rbID);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *rbID);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return *rbID != 0 && *fbID != 0;
    }

    const char* vsSrc =
        "#version 330 core\n"
        "in vec2 vpos;\n"
        "in vec2 tpos;\n"
        "out vec2 texCoord;\n"
        "void main(){\n"
        "    gl_Position = vec4(vpos,0,1);\n"
        "    texCoord = tpos;\n"
        "}\n";

    void setRenderBufferAll(RenderBuffer& buffer, const wxColor& colour)
    {
        for (int y = 0; y < buffer.BufferHt; ++y)
        {
            for (int x = 0; x < buffer.BufferWi; ++x)
            {
                buffer.SetPixel(x, y, colour);
            }
        }
    }
}

ShaderEffect::ShaderEffect(int i) : RenderableEffect(i, "Shader", shader_16_xpm, shader_24_xpm, shader_32_xpm, shader_48_xpm, shader_64_xpm)
{

}

ShaderEffect::~ShaderEffect()
{

}

wxPanel *ShaderEffect::CreatePanel(wxWindow *parent)
{
    return new ShaderPanel(parent);
}

bool ShaderEffect::needToAdjustSettings(const std::string &version)
{
    return false;
}

void ShaderEffect::adjustSettings(const std::string &version, Effect *effect, bool removeDefaults)
{
    //SettingsMap &settings = effect->GetSettings();

    // also give the base class a chance to adjust any settings
    if (RenderableEffect::needToAdjustSettings(version))
    {
        RenderableEffect::adjustSettings(version, effect, removeDefaults);
    }
}

std::list<std::string> ShaderEffect::CheckEffectSettings(const SettingsMap& settings, AudioManager* media, Model* model, Effect* eff)
{
    std::list<std::string> res;

    return res;
}

ShaderConfig* ShaderEffect::ParseShader(const std::string& filename)
{
    if (!wxFile::Exists(filename)) return nullptr;

    wxFile f(filename);
    if (!f.IsOpened()) return nullptr;

    wxString code;
    f.ReadAll(&code);
    f.Close();

    if (code == "") return nullptr;

    wxRegEx re("\\/\\*(.*?)\\*\\/", wxRE_ADVANCED);

    if (!re.Matches(code)) return nullptr;

    return new ShaderConfig(filename, code, re.GetMatch(code, 1));
}

void ShaderEffect::SetDefaultParameters()
{

}

void ShaderEffect::RemoveDefaults(const std::string &version, Effect *effect)
{
    RenderableEffect::RemoveDefaults(version, effect);
}

class ShaderRenderCache : public EffectRenderCache {

public:
    ShaderRenderCache() { _shaderConfig = nullptr; }
    virtual ~ShaderRenderCache()
    {
        if (_shaderConfig != nullptr) delete _shaderConfig;
    }

    ShaderConfig* _shaderConfig = nullptr;
    bool s_shadersInit = false;
    unsigned s_vertexArrayId = 0;
    unsigned s_vertexBufferId = 0;
    unsigned s_fbId = 0;
    unsigned s_rbId = 0;
    unsigned s_rbTex = 0;
    unsigned s_programId = 0;
    int s_rbWidth = 0;
    int s_rbHeight = 0;

    void InitialiseShaderConfig(const wxString& filename)
    {
        if (_shaderConfig != nullptr) delete _shaderConfig;
        _shaderConfig = ShaderEffect::ParseShader(filename);
    }
};

void ShaderEffect::Render(Effect *eff, SettingsMap &SettingsMap, RenderBuffer &buffer)
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    // Bail out right away if we don't have the necessary OpenGL support
   if ( !OpenGLShaders::HasFramebufferObjects() || !OpenGLShaders::HasShaderSupport() )
   {
      setRenderBufferAll(buffer, *wxCYAN);
      logger_base.error( "ShaderEffect::Render() - missing OpenGL support!!" );
      return;
   }

    ShaderRenderCache* cache = (ShaderRenderCache*)buffer.infoCache[id];
    if (cache == nullptr) {
        cache = new ShaderRenderCache();
        buffer.infoCache[id] = cache;
    }

    // This object has all the data from the json in the .fs file
    ShaderConfig*& _shaderConfig = cache->_shaderConfig;
    bool&     s_shadersInit = cache->s_shadersInit;
    unsigned& s_vertexArrayId = cache->s_vertexArrayId;
    unsigned& s_vertexBufferId = cache->s_vertexBufferId;
    unsigned& s_fbId = cache->s_fbId;
    unsigned& s_rbId = cache->s_rbId;
    unsigned& s_programId = cache->s_programId;
    unsigned& s_rbTex = cache->s_rbTex;
    int&      s_rbWidth = cache->s_rbWidth;
    int&      s_rbHeight = cache->s_rbHeight;

    if (buffer.needToInit)
    {
        buffer.needToInit = false;
        cache->InitialiseShaderConfig(SettingsMap.Get("0FILEPICKERCTRL_IFS", ""));
        if (_shaderConfig != nullptr)
        {
            recompileFromShaderConfig(_shaderConfig, s_programId);
            if (s_programId != 0)
            {
                logger_base.debug("Fragment shader %s compiled successfully.", (const char*)_shaderConfig->GetFilename().c_str());
            }
        }
    }

    // if there is no config then we should paint it red ... just like the video effect
    if ( _shaderConfig == nullptr)
    {
       setRenderBufferAll( buffer, *wxRED);
       return;
    }
    else if (s_programId == 0)
    {
        setRenderBufferAll(buffer, *wxYELLOW);
        return;
    }

    // ***********************************************************************************************************
    // todo is there more of this code we could add to the needtoinit case as this only happens on the first frame
    // ***********************************************************************************************************

    ShaderPanel *p = (ShaderPanel *)panel;
    p->_preview->SetCurrentGLContext();

    // We re-use the same framebuffer for rendering all the shader effects
    sizeForRenderBuffer( buffer, s_shadersInit, s_vertexArrayId, s_vertexBufferId, s_rbId, s_fbId, s_rbTex, s_rbWidth, s_rbHeight );

    glBindFramebuffer( GL_FRAMEBUFFER, s_fbId );
    glViewport( 0, 0, buffer.BufferWi, buffer.BufferHt );

    glClearColor( 0.f, 0.f, 0.f, 0.f );
    glClear( GL_COLOR_BUFFER_BIT );

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, s_rbTex );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0,0, buffer.BufferWi,buffer.BufferHt, GL_RGBA, GL_UNSIGNED_BYTE, &buffer.pixels[0] );
    glBindVertexArray( s_vertexArrayId );
    glBindBuffer( GL_ARRAY_BUFFER, s_vertexBufferId );

    GLuint programId = s_programId;
    glUseProgram( programId );

    int colourIndex = 0;
    int loc = glGetUniformLocation( programId, "RENDERSIZE" );
    if (loc >= 0) {
        glUniform2f(loc, buffer.BufferWi, buffer.BufferHt);
    }
    else {
        if (buffer.curPeriod == buffer.curEffStartPer && _shaderConfig->HasRendersize())
        {
            logger_base.warn("Unable to bind to RENDERSIZE\n%s", (const char *)_shaderConfig->GetCode().c_str());
        }
    }
    loc = glGetUniformLocation( programId, "TIME" );
    if (loc >= 0) {
        //glUniform1f(loc, (float)(buffer.curPeriod - buffer.curEffStartPer) / (1000.0f / (float)buffer.frameTimeInMs));
        glUniform1f(loc, (float)(buffer.curPeriod - buffer.curEffStartPer) / (1000.0 / (float)buffer.frameTimeInMs));
    } 
    else {
        if (buffer.curPeriod == buffer.curEffStartPer && _shaderConfig->HasTime())
            logger_base.error("Unable to bind to TIME\n%s", (const char*)_shaderConfig->GetCode().c_str());
    }
    loc = glGetUniformLocation( programId, "texSampler" );
    if (loc >= 0) {
        glUniform1i(loc, 0);
    } 
    else {
        if (buffer.curPeriod == buffer.curEffStartPer)
            logger_base.warn("Unable to bind to texSampler");
    }
    float oset = buffer.GetEffectTimeIntervalPosition();
    for (auto it : _shaderConfig->GetParms())
    {
        loc = glGetUniformLocation(programId, it._name.c_str());
        if (loc >= 0)
        {
            switch (it._type)
            {
            case ShaderParmType::SHADER_PARM_FLOAT:
            {
                float f = GetValueCurveDouble(it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_VALUECURVE), it._default * 100.0, SettingsMap, oset, it._min * 100.0, it._max * 100.0, buffer.GetStartTimeMS(), buffer.GetEndTimeMS(), 1) / 100.0;
                glUniform1f(loc, f);
                break;
            }
            case ShaderParmType::SHADER_PARM_POINT2D:
            {
                float x = GetValueCurveDouble(it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_VALUECURVE) + "X", it._defaultPt.x * 100, SettingsMap, oset, it._minPt.x * 100, it._maxPt.x * 100, buffer.GetStartTimeMS(), buffer.GetEndTimeMS(), 1) / 100.0;
                float y = GetValueCurveDouble(it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_VALUECURVE) + "Y", it._defaultPt.y * 100, SettingsMap, oset, it._minPt.y * 100, it._maxPt.y * 100, buffer.GetStartTimeMS(), buffer.GetEndTimeMS(), 1) / 100.0;
                glUniform2f(loc, x, y);
                break;
            }
            case ShaderParmType::SHADER_PARM_BOOL:
            {
                bool b = SettingsMap.GetBool(it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_CHECKBOX));
                glUniform1f(loc, b);
                break;
            }
            case ShaderParmType::SHADER_PARM_LONGCHOICE:
            {
                long l = it.EncodeChoice(SettingsMap[it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_CHOICE)]);
                glUniform1i(loc, l);
                break;
            }
            case ShaderParmType::SHADER_PARM_LONG:
            {
                long l = GetValueCurveInt(it.GetUndecoratedId(ShaderCtrlType::SHADER_CTRL_VALUECURVE), it._default, SettingsMap, oset, it._min, it._max, 
                    buffer.GetStartTimeMS(), buffer.GetEndTimeMS(), 1);
                glUniform1i(loc, l);
                break;
            }
            case ShaderParmType::SHADER_PARM_COLOUR:
            {
                xlColor c = buffer.palette.GetColor(colourIndex);
                colourIndex++;
                if (colourIndex > buffer.GetColorCount()) colourIndex = 0;
                glUniform4f(loc, (float)c.red / 255.0, (float)c.green / 255.0, (float)c.blue / 255.0, 1.0);
                break;
            }            
            default:
                logger_base.warn("No binding supported for %s ... we have more work to do.", (const char*)it._name.c_str());
                break;
            }
        }
        else
        {
            if (buffer.curPeriod == buffer.curEffStartPer)
                logger_base.warn("Unable to bind to %s", (const char*)it._name.c_str());
        }
    }

      GLuint vattrib = glGetAttribLocation( programId, "vpos" );
      glVertexAttribPointer( vattrib, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTex), reinterpret_cast<void *>( offsetof(VertexTex, v) ) );
      glEnableVertexAttribArray( vattrib );

      GLuint tattrib = glGetAttribLocation( programId, "tpos" );
      glVertexAttribPointer( tattrib, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTex), reinterpret_cast<void *>( offsetof(VertexTex, t ) ) );
      glEnableVertexAttribArray( tattrib );

      glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

      glDisableVertexAttribArray( vattrib );
      glDisableVertexAttribArray( tattrib );

      glBindVertexArray( 0 );
      glBindBuffer( GL_ARRAY_BUFFER, 0 );

      xlColorVector& cv( buffer.pixels );
      glReadPixels( 0, 0, buffer.BufferWi, buffer.BufferHt, GL_RGBA, GL_UNSIGNED_BYTE, &cv[0] );
}

void ShaderEffect::sizeForRenderBuffer(const RenderBuffer& rb,
    bool& s_shadersInit,
    unsigned& s_vertexArrayId, unsigned& s_vertexBufferId, unsigned& s_rbId, unsigned& s_fbId,
    unsigned& s_rbTex, int& s_rbWidth, int& s_rbHeight)
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    if (!s_shadersInit)
    {
        VertexTex vt[4] =
        {
           { {  1.f, -1.f }, { 1.f, 0.f } },
           { { -1.f, -1.f }, { 0.f, 0.f } },
           { {  1.f,  1.f }, { 1.f, 1.f } },
           { { -1.f,  1.f }, { 0.f, 1.f } }
        };
        glGenVertexArrays(1, &s_vertexArrayId);
        glGenBuffers(1, &s_vertexBufferId);

        glBindVertexArray(s_vertexArrayId);
        glBindBuffer(GL_ARRAY_BUFFER, s_vertexBufferId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexTex[4]), vt, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        GLenum err = glGetError();
        if ( err != GL_NO_ERROR )
        {
           logger_base.error( "ShaderEffect::sizeForRenderBuffer() - Error with vertex array - %d", err );
        }

        createOpenGLRenderBuffer(rb.BufferWi, rb.BufferHt, &s_rbId, &s_fbId);
        if ( ( err = glGetError()) != GL_NO_ERROR )
        {
           logger_base.error( "ShaderEffect::sizeForRenderBuffer() - Error creating framebuffer - %d", err );
        }

        s_rbTex = RenderBufferTexture(rb.BufferWi, rb.BufferHt);
        if ( ( err = glGetError()) != GL_NO_ERROR )
        {
           logger_base.error( "ShaderEffect::sizeForRenderBuffer() - Error creating renderbuffer texture - %d", err );
        }

        s_rbWidth = rb.BufferWi;
        s_rbHeight = rb.BufferHt;
        s_shadersInit = true;
    }
    else if (rb.BufferWi > s_rbWidth || rb.BufferHt > s_rbHeight)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (s_fbId)
            glDeleteFramebuffers(1, &s_fbId);
        if (s_rbId)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glDeleteRenderbuffers(1, &s_rbId);
        }
        if (s_rbTex)
            glDeleteTextures(1, &s_rbTex);
        createOpenGLRenderBuffer(rb.BufferWi, rb.BufferHt, &s_rbId, &s_fbId);
        GLenum err = glGetError();
        if ( err != GL_NO_ERROR )
        {
           logger_base.error( "ShaderEffect::sizeForRenderBuffer() - Error recreating framebuffer - %d", err );
        }
        s_rbTex = RenderBufferTexture(rb.BufferWi, rb.BufferHt);;
        if ( ( err = glGetError()) != GL_NO_ERROR )
        {
           logger_base.error( "ShaderEffect::sizeForRenderBuffer() - Error recreating renderbuffer texture - %d", err );
        }

        s_rbWidth = rb.BufferWi;
        s_rbHeight = rb.BufferHt;
    }
}

void ShaderEffect::recompileFromShaderConfig( const ShaderConfig* cfg, unsigned& s_programId)
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));

    s_programId = OpenGLShaders::compile( vsSrc, cfg->GetCode() );
   if (s_programId == 0)
   {
       logger_base.error("Failed to compile shader program %s", (const char *)cfg->GetFilename().c_str());
   }
}

ShaderConfig::ShaderConfig(const wxString& filename, const wxString& code, const wxString& json) : _filename(filename)
{
    wxJSONReader reader;
    wxJSONValue root;
    reader.Parse(json, &root);
    _description = root["DESCRIPTION"].AsString();
    wxJSONValue inputs = root["INPUTS"];
    wxString canvasImgName;
    for (int i = 0; i < inputs.Size(); i++)
    {
        wxString type = inputs[i]["TYPE"].AsString();
        if (type == "float")
        {
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_FLOAT,
                    (float)(inputs[i].HasMember("MIN") ? wxAtof(inputs[i]["MIN"].AsString()) : 0.0),
                    (float)(inputs[i].HasMember("MAX") ? wxAtof(inputs[i]["MAX"].AsString()) : 0.0),
                    (float)(inputs[i].HasMember("DEFAULT") ? wxAtof(inputs[i]["DEFAULT"].AsString()) : 0.0)
                ));
        }
        else if (type == "long")
        {
            if (inputs[i].HasMember("MIN"))
            {
                _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_LONG,
                    (float)(inputs[i].HasMember("MIN") ? wxAtol(inputs[i]["MIN"].AsString()) : 0.0),
                    (float)(inputs[i].HasMember("MAX") ? wxAtol(inputs[i]["MAX"].AsString()) : 0.0),
                    (float)(inputs[i].HasMember("DEFAULT") ? wxAtol(inputs[i]["DEFAULT"].AsString()) : 0.0)
                ));
            }
            else if (inputs[i].HasMember("LABELS") && inputs[i].HasMember("VALUES"))
            {
                _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_LONGCHOICE,
                    0.0f,
                    0.0f,
                    (float)(inputs[i].HasMember("DEFAULT") ? wxAtol(inputs[i]["DEFAULT"].AsString()) : 0.0)
                ));
                auto ls = inputs[i]["LABELS"];
                auto vs = inputs[i]["VALUES"];
                int no = std::min(ls.Size(), vs.Size());
                for (int i = 0; i < no; i++)
                {
                    _parms.back()._valueOptions[vs[i].AsInt()] = ls[i].AsString();
                }
            }
            else
            {
                wxASSERT(false);
            }
        }
        else if (type == "color")
        {
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_COLOUR
                ));
        }
        else if (type == "audio")
        {
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_AUDIO
                ));
        }
        else if (type == "audiofft")
        {
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_AUDIOFFT
                ));
        }
        else if (type == "bool")
        {
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_BOOL,
                    0.0f,
                    0.0f,
                    (float)(inputs[i].HasMember("DEFAULT") ? wxAtof(inputs[i]["DEFAULT"].AsString()) : 0.0f)
                ));
        }
        else if (type == "point2D")
        {
            wxRealPoint minPt = wxRealPoint(
                inputs[i].HasMember("MIN") ? (inputs[i]["MIN"][0].IsDouble() ? inputs[i]["MIN"][0].AsDouble() : inputs[i]["MIN"][0].AsInt()) : 0.0f,
                inputs[i].HasMember("MIN") ? (inputs[i]["MIN"][1].IsDouble() ? inputs[i]["MIN"][1].AsDouble() : inputs[i]["MIN"][1].AsInt()) : 0.0f
            );
            wxRealPoint maxPt = wxRealPoint(
                inputs[i].HasMember("MAX") ? (inputs[i]["MAX"][0].IsDouble() ? inputs[i]["MAX"][0].AsDouble() : inputs[i]["MAX"][0].AsInt()) : 1.0f,
                inputs[i].HasMember("MAX") ? (inputs[i]["MAX"][1].IsDouble() ? inputs[i]["MAX"][1].AsDouble() : inputs[i]["MAX"][1].AsInt()) : 1.0f
            );
            wxRealPoint defPt = wxRealPoint(
                inputs[i].HasMember("DEFAULT") ? (inputs[i]["DEFAULT"][0].IsDouble() ? inputs[i]["DEFAULT"][0].AsDouble() : inputs[i]["DEFAULT"][0].AsInt()) : 0.0f,
                inputs[i].HasMember("DEFAULT") ? (inputs[i]["DEFAULT"][1].IsDouble() ? inputs[i]["DEFAULT"][1].AsDouble() : inputs[i]["DEFAULT"][1].AsInt()) : 0.0f
            );
            _parms.push_back(ShaderParm(
                    inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
                    inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
                    ShaderParmType::SHADER_PARM_POINT2D,
                    minPt,
                    maxPt,
                    defPt
                ));
        }
        else if (type == "image")
        {
            // ignore these as we will use the existing buffer content
            //_parms.push_back({
            //        inputs[i].HasMember("NAME") ? inputs[i]["NAME"].AsString() : "",
            //        inputs[i].HasMember("LABEL") ? inputs[i]["LABEL"].AsString() : "",
            //        ShaderParmType::SHADER_PARM_IMAGE,
            //        0.0f,
            //        0.0f,
            //        0.0f
            //    });
            if ( inputs[i].HasMember( "NAME" ) )
            {
               canvasImgName = inputs[i]["NAME"].AsString();
               //_canvasMode = true;
            }
        }
        else if (type == "event")
        {
            // ignore these
        }
        else
        {
            wxASSERT(false);
        }
    }
    wxJSONValue passes = root["PASSES"];
    for (int i = 0; i < passes.Size(); i++)
    {
        _passes.push_back({
            inputs[i].HasMember("TARGET") ? inputs[i]["TARGET"].AsString() : "",
            passes[i].HasMember("PERSISTENT") ? passes[i]["PERSISTENT"].AsString() == "true" : false
            });
    }

    _hasRendersize = Contains(code, "RENDERSIZE");
    _hasTime = Contains(code, "TIME");

    // The shader code needs declarations for the uniforms that we silently set with each call to Render()
    // and the uniforms that correspond to user-visible settings
    wxString prependText = "#version 330\n\nuniform float TIME;\nuniform vec2 RENDERSIZE;\nuniform sampler2D texSampler;\n";

    for ( auto p : _parms )
    {
       wxString name( p._name );
       wxString str;
       switch ( p._type )
       {
          case ShaderParmType::SHADER_PARM_FLOAT:
          {
             str = wxString::Format( "uniform float %s;\n", name );
             prependText += str;
             break;
          }
          case ShaderParmType::SHADER_PARM_BOOL:
          {
              str = wxString::Format("uniform bool %s;\n", name);
              prependText += str;
              break;
          }
          case ShaderParmType::SHADER_PARM_LONG:
          case ShaderParmType::SHADER_PARM_LONGCHOICE:
          {
              str = wxString::Format("uniform int %s;\n", name);
              prependText += str;
              break;
          }
          case ShaderParmType::SHADER_PARM_POINT2D:
          {
             str = wxString::Format( "uniform vec2 %s;\r\n", name );
             prependText += str;
             break;
          }
          case ShaderParmType::SHADER_PARM_COLOUR:
          {
              str = wxString::Format("uniform vec4 %s;\r\n", name);
              prependText += str;
              break;
          }
          default:
          {
             // rest of these are un-implemented currently
          }
       }
   }

   size_t pos = code.find( "*/");
   wxString shaderCode = ( pos != wxString::npos ) ? code.substr( pos + 2 ) : code;
   if ( !canvasImgName.empty() )
   {
      shaderCode.Replace( canvasImgName, "texSampler" );
      shaderCode.Replace( "IMG_NORM_PIXEL", "texture" );
      _canvasMode = true;
   }
   _code = prependText + shaderCode;

   #if 0
   std::ofstream s( "C:\\Temp\\temp.txt" );
   if ( s.good() )
   {
      s << _code;
      s.close();
   }
   #endif
}
