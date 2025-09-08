#include "compute-shader.h"

ComputeShader::ComputeShader(const char* code_path)
{
    // 1. retrieve the vertex/fragment source code from filePath
    std::string code;
    std::string fragment_code;
    std::ifstream shader_file;
    // ensure ifstream objects can throw exceptions:
    shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shader_file.open(code_path);
        std::stringstream shader_stream;
        shader_stream << shader_file.rdbuf();
        shader_file.close();
        code = shader_stream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }

    // compute shader
    const char* c_shader_code = code.c_str();
    unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &c_shader_code, NULL);
    glCompileShader(compute);
    checkCompileErrors_(compute, "COMPUTE");

    // shader Program
    ID = glCreateProgram();
    glAttachShader(ID, compute);
    glLinkProgram(ID);
    checkCompileErrors_(ID, "PROGRAM");
    glDeleteShader(compute);
}

ComputeShader::ComputeShader(unsigned int id) : ID(id) {}