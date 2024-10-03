/*
 * Copyright (c) Catalin Ionescu 2024. All rights reserved.
 * Copyright (c) Robert Bengulescu 2024. All rights reserved.
 * Copyright (c) Traian Avram 2024. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <Renderer/Platform/D3D11/D3D11Renderer.h>
#include <Renderer/Platform/D3D11/D3D11Shader.h>
#include <d3dcompiler.h>

namespace CaveGame
{

D3D11Shader::D3D11Shader(const ShaderDescription& description)
{
    m_shader_modules.set_capacity(description.stages.count());
    for (const ShaderStageDescription& stage_description : description.stages)
    {
        bool stage_already_exists = false;
        for (const auto& shader_module : m_shader_modules)
        {
            if (shader_module.stage == stage_description.stage)
            {
                stage_already_exists = true;
                break;
            }
        }

        if (stage_already_exists)
        {
            // The description is for a shader stage that already exists, thus it is ignored.
            // TODO: Inform the user about this situation, as it is most likely a bug.
            continue;
        }

        ShaderModule shader_module = create_shader_module(stage_description);
        m_shader_modules.add(move(shader_module));
    }
}

D3D11Shader::~D3D11Shader()
{
    for (ShaderModule& shader_module : m_shader_modules)
        CAVE_D3D11_RELEASE(shader_module.handle);

    m_shader_modules.clear_and_shrink();
}

IUnknown* D3D11Shader::get_shader_module(ShaderStage stage)
{
    for (ShaderModule& shader_module : m_shader_modules)
    {
        if (shader_module.stage == stage)
            return shader_module.handle;
    }

    // The given shader stage doesn't exist.
    return nullptr;
}

NODISCARD ALWAYS_INLINE static const char* get_shader_entrypoint_name(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return "cave_vertex_main";
        case ShaderStage::Fragment: return "cave_fragment_main";
    }

    CAVE_ASSERT(false);
    return nullptr;
}

NODISCARD ALWAYS_INLINE static const char* get_shader_target(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return "vs_5_0";
        case ShaderStage::Fragment: return "ps_5_0";
    }

    CAVE_ASSERT(false);
    return nullptr;
}

D3D11Shader::ShaderCompilationResult D3D11Shader::compile_shader_module(ShaderStage stage, StringView source_code)
{
    ShaderCompilationResult compilation_result = {};

    ID3DBlob* bytecode_blob = nullptr;
    ID3DBlob* error_message_blob = nullptr;

    const HRESULT result_code = D3DCompile(
        source_code.characters(),
        source_code.byte_count(),
        nullptr,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        get_shader_entrypoint_name(stage),
        get_shader_target(stage),
        0,
        0,
        &bytecode_blob,
        &error_message_blob
    );

    if (bytecode_blob)
    {
        compilation_result.bytecode = Buffer::copy(bytecode_blob->GetBufferPointer(), bytecode_blob->GetBufferSize());
        CAVE_D3D11_RELEASE(bytecode_blob);
    }

    if (error_message_blob)
    {
        const StringView error_message_view =
            StringView::create_from_utf8(static_cast<const char*>(error_message_blob->GetBufferPointer()), error_message_blob->GetBufferSize());
        compilation_result.error_message = error_message_view;
        CAVE_D3D11_RELEASE(error_message_blob);
    }

    compilation_result.result = result_code;
    return compilation_result;
}

D3D11Shader::ShaderModule D3D11Shader::create_shader_module(const ShaderStageDescription& description)
{
    ShaderModule shader_module = {};
    shader_module.stage = description.stage;

    void* bytecode_data = nullptr;
    usize bytecode_size = 0;
    Buffer compiled_bytecode;

    if (description.source_type == ShaderSourceType::SourceCode)
    {
        // No shadersource code has been provided.
        CAVE_ASSERT(!description.source_code.is_empty());

        ShaderCompilationResult compilation_result = compile_shader_module(description.stage, description.source_code);
        if (!compilation_result.error_message.is_empty())
        {
            // TODO: Inform the user about the error messages given by the compilation process.
            CAVE_ASSERT(false);
        }

        // The compilation process failed, even if no error messages were generated.
        CAVE_ASSERT(SUCCEEDED(compilation_result.result));

        compiled_bytecode = move(compilation_result.bytecode);
        bytecode_data = compiled_bytecode.data();
        bytecode_size = compiled_bytecode.byte_count();
    }
    else if (description.source_type == ShaderSourceType::Bytecode)
    {
        // No bytecode data has been provided.
        CAVE_ASSERT(description.source_bytecode_size > 0);

        bytecode_data = description.source_bytecode_data;
        bytecode_size = description.source_bytecode_size;
    }
    else
    {
        // Invalid code path.
        CAVE_VERIFY(false);
    }

    switch (description.stage)
    {
        case ShaderStage::Vertex:
        {
            ID3D11VertexShader* vertex_shader = nullptr;
            const HRESULT vertex_shader_creation_result =
                D3D11Renderer::get_device()->CreateVertexShader(bytecode_data, bytecode_size, nullptr, &vertex_shader);
            CAVE_ASSERT(SUCCEEDED(vertex_shader_creation_result));
            shader_module.handle = vertex_shader;
        }
        break;

        case ShaderStage::Fragment:
        {
            ID3D11PixelShader* fragment_shader = nullptr;
            const HRESULT fragment_shader_creation_result =
                D3D11Renderer::get_device()->CreatePixelShader(bytecode_data, bytecode_size, nullptr, &fragment_shader);
            CAVE_ASSERT(SUCCEEDED(fragment_shader_creation_result));
            shader_module.handle = fragment_shader;
        }
        break;
    }

    // The compiled bytecode buffer is not empty only when the shader source type was set to `SourceCode`.
    compiled_bytecode.release();
    return shader_module;
}

} // namespace CaveGame
