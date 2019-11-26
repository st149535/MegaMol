/*
 * OverlayRenderer.cpp
 *
 * Copyright (C) 2019 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "OverlayRenderer.h"


using namespace megamol;
using namespace megamol::core;
using namespace megamol::gui;


OverlayRenderer::OverlayRenderer(void)
    : view::RendererModule<view::CallRender3D_2>()
    , paramMode("mode", "Overlay mode.")
    , paramAnchor("anchor", "Anchor of overlay.")
    , paramCustomPosition("position_offset", "Custom relative position offset in respect to selected anchor.")
    , paramFileName("texture::file_name", "The file name of the texture.")
    , paramRelativeWidth("texture::relative_width", "Relative screen space width of texture.")
    , paramPrefix("parameter::prefix", "The parameter value prefix.")
    , paramSufix("parameter::sufix", "The parameter value sufix.")
    , paramParameterName("parameter::name", "The full parameter name, e.g. '::Project_1::View3D_21::anim::time'")
    , paramText("label::text", "The displayed text.")
    , paramFontName("font::name", "The font name.")
    , paramFontSize("font::size", "The font size.")
    , paramFontColor("font::color", "The font color.")
    , m_texture()
    , m_shader()
    , m_font(nullptr)
    , m_media_buttons()
    , m_parameter_ptr(nullptr)
    , m_viewport()
    , m_current_rectangle({0.0f, 0.0f, 0.0f, 0.0f}) {

    this->MakeSlotAvailable(&this->chainRenderSlot);
    this->MakeSlotAvailable(&this->renderSlot);

    param::EnumParam* mep = new param::EnumParam(Mode::TEXTURE);
    mep->SetTypePair(Mode::TEXTURE, "Texture");
    mep->SetTypePair(Mode::MEDIA_BUTTONS, "Media Buttons");
    mep->SetTypePair(Mode::PARAMETER, "Parameter");
    mep->SetTypePair(Mode::LABEL, "Label");
    this->paramMode << mep;
    this->paramMode.SetUpdateCallback(this, &OverlayRenderer::onToggleMode);
    this->MakeSlotAvailable(&this->paramMode);

    param::EnumParam* aep = new param::EnumParam(Anchor::ALIGN_LEFT_TOP);
    aep->SetTypePair(Anchor::ALIGN_LEFT_TOP, "Left Top");
    aep->SetTypePair(Anchor::ALIGN_LEFT_MIDDLE, "Left Middle");
    aep->SetTypePair(Anchor::ALIGN_LEFT_BOTTOM, "Left Bottom");
    aep->SetTypePair(Anchor::ALIGN_CENTER_TOP, "Center Top");
    aep->SetTypePair(Anchor::ALIGN_CENTER_MIDDLE, "Center Middle");
    aep->SetTypePair(Anchor::ALIGN_CENTER_BOTTOM, "Center Bottom");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_TOP, "Right Top");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_MIDDLE, "Right Middle");
    aep->SetTypePair(Anchor::ALIGN_RIGHT_BOTTOM, "Right Bottom");
    this->paramAnchor << aep;
    this->paramAnchor.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramAnchor);

    this->paramCustomPosition << new param::Vector2fParam(vislib::math::Vector<float, 2>(0.0f, 0.0f),
        vislib::math::Vector<float, 2>(0.0f, 0.0f), vislib::math::Vector<float, 2>(100.0f, 100.0f));
    this->paramCustomPosition.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramCustomPosition);

    // Texture Mode
    this->paramFileName << new param::FilePathParam("");
    this->paramFileName.SetUpdateCallback(this, &OverlayRenderer::onTextureFileName);
    this->MakeSlotAvailable(&this->paramFileName);

    this->paramRelativeWidth << new param::FloatParam(25.0f, 0.0f, 100.0f);
    this->paramRelativeWidth.SetUpdateCallback(this, &OverlayRenderer::onTriggerRecalcRectangle);
    this->MakeSlotAvailable(&this->paramRelativeWidth);

    // Parameter Mode
    this->paramPrefix << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramPrefix);

    this->paramSufix << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramSufix);

    this->paramParameterName << new param::StringParam("");
    this->paramParameterName.SetUpdateCallback(this, &OverlayRenderer::onParameterName);
    this->MakeSlotAvailable(&this->paramParameterName);

    // Label Mode
    this->paramText << new param::StringParam("");
    this->MakeSlotAvailable(&this->paramText);

    // Font Settings
    param::EnumParam* fep = new param::EnumParam(utility::SDFFont::FontName::ROBOTO_SANS);
    fep->SetTypePair(utility::SDFFont::FontName::ROBOTO_SANS, "Roboto Sans");
    fep->SetTypePair(utility::SDFFont::FontName::EVOLVENTA_SANS, "Evolventa");
    fep->SetTypePair(utility::SDFFont::FontName::UBUNTU_MONO, "Ubuntu Mono");
    fep->SetTypePair(utility::SDFFont::FontName::VOLLKORN_SERIF, "Vollkorn Serif");
    this->paramFontName << fep;
    this->paramFontName.SetUpdateCallback(this, &OverlayRenderer::onFontName);
    this->MakeSlotAvailable(&this->paramFontName);

    this->paramFontSize << new param::FloatParam(20.0f, 0.0f);
    this->MakeSlotAvailable(&this->paramFontSize);

    this->paramFontColor << new param::ColorParam(0.5f, 0.5f, 0.5f, 1.0f);
    this->MakeSlotAvailable(&this->paramFontColor);
}


OverlayRenderer::~OverlayRenderer(void) { this->Release(); }


void OverlayRenderer::release(void) {

    this->m_font.reset();
    this->m_parameter_ptr = nullptr;
    this->m_shader.Release();
    this->m_texture.tex.Release();
    for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
        this->m_media_buttons[i].tex.Release();
    }
}


bool OverlayRenderer::create(void) {

    return this->onToggleMode(this->paramMode);
    ;
}


bool OverlayRenderer::onToggleMode(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_font.reset();
    this->m_parameter_ptr = nullptr;
    this->m_shader.Release();
    this->m_texture.tex.Release();
    for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
        this->m_media_buttons[i].tex.Release();
    }

    this->setParameterGUIVisibility();

    auto mode = static_cast<Mode>(this->paramMode.Param<param::EnumParam>()->Value());
    switch (mode) {
    case (Mode::TEXTURE): {
        if (!this->loadShader(this->m_shader, "overlay::vertex", "overlay::fragment")) return false;
        // Load current texture from parameter
        this->onTextureFileName(slot);
    } break;
    case (Mode::MEDIA_BUTTONS): {
        /// TODO ...
        // Load media button texutres from hard coded texture file names.
        std::string filename;
        for (size_t i = 0; i < this->m_media_buttons.size(); i++) {
            switch (static_cast<MediaButton>(i)) {
            case (MediaButton::PLAY):
                filename = "media_button_play.png";
                break;
            case (MediaButton::PAUSE):
                filename = "media_button_pause.png";
                break;
            case (MediaButton::STOP):
                filename = "media_button_stop.png";
                break;
            case (MediaButton::REWIND):
                filename = "media_button_rewind.png";
                break;
            case (MediaButton::FAST_FORWARD):
                filename = "media_button_fast-forward.png";
                break;
            }
            if (!this->loadTexture(filename, this->m_media_buttons[i])) return false;
        }
    } break;
    case (Mode::PARAMETER): {
        // Load current parameter by name
        this->onParameterName(slot);
        this->onFontName(slot);
    } break;
    case (Mode::LABEL): {
        // Load current font name from parameter
        this->onFontName(slot);
    } break;
    }

    return true;
}


bool OverlayRenderer::onTextureFileName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_texture.tex.Release();
    std::string filename = std::string(this->paramFileName.Param<param::FilePathParam>()->Value().PeekBuffer());
    if (!this->loadTexture(filename, this->m_texture)) return false;
    this->onTriggerRecalcRectangle(slot);
    return true;
}


bool OverlayRenderer::onFontName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_font.reset();
    auto font_name = static_cast<utility::SDFFont::FontName>(this->paramFontName.Param<param::EnumParam>()->Value());
    this->m_font = std::make_unique<utility::SDFFont>(font_name);
    if (!this->m_font->Initialise(this->GetCoreInstance())) return false;
    return true;
}


bool OverlayRenderer::onParameterName(param::ParamSlot& slot) {

    slot.ResetDirty();
    this->m_parameter_ptr = nullptr;
    auto parameter_name = this->paramParameterName.Param<param::StringParam>()->Value();
    auto parameter = this->GetCoreInstance()->FindParameter(parameter_name, false, false);
    if (parameter.IsNull()) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to find parameter by name. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    auto* float_param = parameter.DynamicCast<param::FloatParam>();
    if (float_param == nullptr) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Parameter is no FloatParam. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    this->m_parameter_ptr = float_param;
    return true;
}

bool OverlayRenderer::onTriggerRecalcRectangle(core::param::ParamSlot& slot) {

    slot.ResetDirty();
    auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());
    auto param_position = this->paramCustomPosition.Param<param::Vector2fParam>()->Value();
    glm::vec2 rel_pos = glm::vec2(param_position.X() / 100.0f, param_position.Y() / 100.0f);
    auto rel_width = this->paramRelativeWidth.Param<param::FloatParam>()->Value() / 100.0f;
    this->m_current_rectangle = this->getScreenSpaceRect(rel_pos, rel_width, anchor, this->m_texture, this->m_viewport);
    return true;
}


void OverlayRenderer::setParameterGUIVisibility(void) {

    Mode mode = static_cast<Mode>(this->paramMode.Param<param::EnumParam>()->Value());

    // Texture Mode
    bool texture_mode = (mode == Mode::TEXTURE);
    this->paramFileName.Param<param::FilePathParam>()->SetGUIVisible(texture_mode);
    this->paramRelativeWidth.Param<param::FloatParam>()->SetGUIVisible(texture_mode);

    // Parameter Mode
    bool parameter_mode = (mode == Mode::PARAMETER);
    this->paramPrefix.Param<param::StringParam>()->SetGUIVisible(parameter_mode);
    this->paramSufix.Param<param::StringParam>()->SetGUIVisible(parameter_mode);
    this->paramParameterName.Param<param::StringParam>()->SetGUIVisible(parameter_mode);

    // Label Mode
    bool label_mode = (mode == Mode::LABEL);
    this->paramText.Param<param::StringParam>()->SetGUIVisible(label_mode);

    // Font Settings
    bool font_settings = (label_mode || parameter_mode);
    this->paramFontName.Param<param::EnumParam>()->SetGUIVisible(font_settings);
    this->paramFontSize.Param<param::FloatParam>()->SetGUIVisible(font_settings);
    this->paramFontColor.Param<param::ColorParam>()->SetGUIVisible(font_settings);
}


bool OverlayRenderer::GetExtents(view::CallRender3D_2& call) {

    auto chainedCall = this->chainRenderSlot.CallAs<view::CallRender3D_2>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        bool retVal = (*chainedCall)(view::AbstractCallRender::FnGetExtents);
        call = *chainedCall;
        return retVal;
    }
    return true;
}


bool OverlayRenderer::Render(view::CallRender3D_2& call) {

    auto leftSlotParent = call.PeekCallerSlot()->Parent();
    std::shared_ptr<const view::AbstractView> viewptr =
        std::dynamic_pointer_cast<const view::AbstractView>(leftSlotParent);
    if (viewptr != nullptr) { // TODO move this behind the fbo magic?
        auto vp = call.GetViewport();
        glViewport(vp.Left(), vp.Bottom(), vp.Width(), vp.Height());
        auto backCol = call.BackgroundColor();
        glClearColor(backCol.x, backCol.y, backCol.z, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // First call chained renderer
    auto* chainedCall = this->chainRenderSlot.CallAs<view::CallRender3D_2>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        if (!(*chainedCall)(view::AbstractCallRender::FnRender)) {
            return false;
        }
    }

    view::Camera_2 cam;
    call.GetCamera(cam);
    glm::ivec4 cam_viewport;
    if (!cam.image_tile().empty()) { /// or better: auto viewport = cr3d_in->GetViewport().GetSize()?
        cam_viewport = glm::ivec4(
            cam.image_tile().left(), cam.image_tile().bottom(), cam.image_tile().width(), cam.image_tile().height());
    } else {
        cam_viewport = glm::ivec4(0, 0, cam.resolution_gate().width(), cam.resolution_gate().height());
    }
    if ((this->m_viewport.x != cam_viewport.z) || (this->m_viewport.y != cam_viewport.w)) {
        this->m_viewport = {cam_viewport.z, cam_viewport.w};
        this->onTriggerRecalcRectangle(this->paramMode);
    }
    glm::mat4 ortho = glm::ortho(
        0.0f, static_cast<float>(this->m_viewport.x), 0.0f, static_cast<float>(this->m_viewport.y), -1.0f, 1.0f);

    auto mode = this->paramMode.Param<param::EnumParam>()->Value();
    switch (mode) {
    case (Mode::TEXTURE): {
        this->drawScreenSpaceBillboard(ortho, this->m_current_rectangle, this->m_texture, this->m_shader);
    } break;
    case (Mode::MEDIA_BUTTONS): {
        /// TODO ...
    } break;
    case (Mode::PARAMETER): {
        auto param_color = this->paramFontColor.Param<param::ColorParam>()->Value();
        glm::vec4 color = glm::vec4(param_color[0], param_color[1], param_color[2], param_color[3]);
        auto font_size = this->paramFontSize.Param<param::FloatParam>()->Value();
        auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());

        auto param_prefix = this->paramPrefix.Param<param::StringParam>()->Value();
        std::string prefix = std::string(param_prefix.PeekBuffer());

        auto param_sufix = this->paramSufix.Param<param::StringParam>()->Value();
        std::string sufix = std::string(param_sufix.PeekBuffer());

        if (this->m_parameter_ptr == nullptr) return false;
        auto value = this->m_parameter_ptr->Value();
        std::stringstream stream;
        stream << std::fixed << std::setprecision(8) << " " << value << " ";
        auto text = prefix + stream.str() + sufix;

        this->drawScreenSpaceText(ortho, (*this->m_font), text, color, font_size, anchor, this->m_current_rectangle);
    } break;
    case (Mode::LABEL): {
        auto param_color = this->paramFontColor.Param<param::ColorParam>()->Value();
        glm::vec4 color = glm::vec4(param_color[0], param_color[1], param_color[2], param_color[3]);
        auto font_size = this->paramFontSize.Param<param::FloatParam>()->Value();
        auto anchor = static_cast<Anchor>(this->paramAnchor.Param<param::EnumParam>()->Value());

        auto param_text = this->paramText.Param<param::StringParam>()->Value();
        std::string text = std::string(param_text.PeekBuffer());

        this->drawScreenSpaceText(ortho, (*this->m_font), text, color, font_size, anchor, this->m_current_rectangle);
    } break;
    }

    return true;
}


void OverlayRenderer::drawScreenSpaceBillboard(
    glm::mat4 ortho, Rectangle rectangle, TextureData& texture, vislib::graphics::gl::GLSLShader& shader) const {

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    texture.tex.Bind();

    shader.Enable();

    glUniformMatrix4fv(shader.ParameterLocation("ortho"), 1, GL_FALSE, glm::value_ptr(ortho));
    glUniform1f(shader.ParameterLocation("left"), rectangle.left);
    glUniform1f(shader.ParameterLocation("right"), rectangle.right);
    glUniform1f(shader.ParameterLocation("top"), rectangle.top);
    glUniform1f(shader.ParameterLocation("bottom"), rectangle.bottom);
    glUniform1i(shader.ParameterLocation("tex"), 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); /// Vertex position is implicitly set via uniform 'lrtb'.

    shader.Disable();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);
}

void OverlayRenderer::drawScreenSpaceText(glm::mat4 ortho, megamol::core::utility::SDFFont& font,
    const std::string& text, glm::vec4 color, float size, Anchor anchor, Rectangle rectangle) const {

    float x = rectangle.left;
    float y = rectangle.top;
    float z = -1.0f;

    // Font rendering takes matrices from OpenGL stack
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glLoadMatrixf(glm::value_ptr(ortho));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    font.DrawString(glm::value_ptr(color), x, y, z, size, false, text.c_str(), anchor);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}


OverlayRenderer::Rectangle OverlayRenderer::getScreenSpaceRect(
    glm::vec2 rel_pos, float rel_width, Anchor anchor, const TextureData& texture, glm::ivec2 viewport) const {

    Rectangle rectangle = {0.0f, 0.0f, 0.0f, 0.0f};

    float tex_aspect = static_cast<float>(texture.width) / static_cast<float>(texture.height);
    float viewport_aspect = static_cast<float>(viewport.x) / static_cast<float>(viewport.y);
    float rel_height = rel_width / tex_aspect * viewport_aspect;

    switch (anchor) {
    case (Anchor::ALIGN_LEFT_TOP): {
        rectangle.left = rel_pos.x;
        rectangle.right = rectangle.left + rel_width;
        rectangle.top = 1.0f - rel_pos.y;
        rectangle.bottom = 1.0f - rel_pos.y - rel_height;
    } break;

    case (Anchor::ALIGN_LEFT_MIDDLE): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_LEFT_BOTTOM): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_CENTER_TOP): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_CENTER_MIDDLE): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_CENTER_BOTTOM): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_RIGHT_TOP): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_RIGHT_MIDDLE): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    case (Anchor::ALIGN_RIGHT_BOTTOM): {
        // rectangle.left = rel_pos.x;
        // rectangle.right = rectangle.left + rel_width;
        // rectangle.top = 1.0 - rel_pos.y;
        // rectangle.bottom = 1.0 - rel_pos.y - rel_height;
    } break;
    }

    rectangle.left *= viewport.x;
    rectangle.right *= viewport.x;
    rectangle.top *= viewport.y;
    rectangle.bottom *= viewport.y;

    return rectangle;
}


bool OverlayRenderer::loadTexture(const std::string& filename, TextureData& texture) const {

    if (filename.empty()) return false;
    if (texture.tex.IsValid()) texture.tex.Release();

    static vislib::graphics::BitmapImage img;
    static sg::graphics::PngBitmapCodec pbc;
    pbc.Image() = &img;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    void* buf = nullptr;
    size_t size = 0;

    size = utility::ResourceWrapper::LoadResource(
        this->GetCoreInstance()->Configuration(), vislib::StringA(filename.c_str()), (void**)(&buf));
    if (size == 0) {
        size = this->loadRawFile(filename, &buf);
    }

    if (size > 0) {
        if (pbc.Load(buf, size)) {
            img.Convert(vislib::graphics::BitmapImage::TemplateByteRGBA);
            if (texture.tex.Create(img.Width(), img.Height(), false, img.PeekDataAs<BYTE>(), GL_RGBA) != GL_NO_ERROR) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                    "Unable to create texture: %s [%s, %s, line %d]\n", filename.c_str(), __FILE__, __FUNCTION__,
                    __LINE__);
                ARY_SAFE_DELETE(buf);
                return false;
            }
            texture.width = img.Width();
            texture.height = img.Height();
            texture.tex.Bind();
            glGenerateMipmap(GL_TEXTURE_2D);
            texture.tex.SetFilter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR);
            // texture.tex.SetFilter(GL_LINEAR, GL_LINEAR);
            texture.tex.SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            ARY_SAFE_DELETE(buf);
        } else {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to read texture: %s [%s, %s, line %d]\n", filename.c_str(), __FILE__, __FUNCTION__, __LINE__);
            ARY_SAFE_DELETE(buf);
            return false;
        }
    } else {
        ARY_SAFE_DELETE(buf);
        return false;
    }

    return true;
}


bool OverlayRenderer::loadShader(
    vislib::graphics::gl::GLSLShader& io_shader, const std::string& vert_name, const std::string& frag_name) const {

    io_shader.Release();
    vislib::graphics::gl::ShaderSource vert, frag;
    try {
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource(
                vislib::StringA(vert_name.c_str()), vert)) {
            return false;
        }
        if (!this->GetCoreInstance()->ShaderSourceFactory().MakeShaderSource(
                vislib::StringA(frag_name.c_str()), frag)) {
            return false;
        }
        if (!io_shader.Create(vert.Code(), vert.Count(), frag.Code(), frag.Count())) {
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                "Unable to compile: Unknown error [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }
    } catch (vislib::graphics::gl::AbstractOpenGLShader::CompileException ce) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader (@%s): %s [%s, %s, line %d]\n",
            vislib::graphics::gl::AbstractOpenGLShader::CompileException::CompileActionName(ce.FailedAction()),
            ce.GetMsgA(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (vislib::Exception e) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader: %s [%s, %s, line %d]\n", e.GetMsgA(), __FILE__, __FUNCTION__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
            "Unable to compile shader: Unknown exception [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


size_t OverlayRenderer::loadRawFile(std::string name, void** outData) const {

    *outData = nullptr;

    vislib::StringW filename = static_cast<vislib::StringW>(name.c_str());
    if (filename.IsEmpty()) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unable to load file: No file name given. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    if (!vislib::sys::File::Exists(filename)) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": Not existing. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    size_t size = static_cast<size_t>(vislib::sys::File::GetSize(filename));
    if (size < 1) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": File is empty. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    vislib::sys::FastFile f;
    if (!f.Open(filename, vislib::sys::File::READ_ONLY, vislib::sys::File::SHARE_READ, vislib::sys::File::OPEN_ONLY)) {
        vislib::sys::Log::DefaultLog.WriteError("Unable to load file \"%s\": Unable to open file. [%s, %s, line %d]\n",
            name.c_str(), __FILE__, __FUNCTION__, __LINE__);
        return 0;
    }

    *outData = new BYTE[size];
    size_t num = static_cast<size_t>(f.Read(*outData, size));
    if (num != size) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unable to load file \"%s\": Unable to read whole file. [%s, %s, line %d]\n", name.c_str(), __FILE__,
            __FUNCTION__, __LINE__);
        ARY_SAFE_DELETE(*outData);
        return 0;
    }

    return num;
}
