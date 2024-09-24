#ifndef DRAWUTIL_H
#define DRAWUTIL_H

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "shader.h"

#include <iostream>

namespace drawUtils {
    std::vector<float> line_data{};
    unsigned int line_shader;
    glm::vec3 line_color(0., 0., 0.);

    unsigned int initLineShader() {
        const char* v_shader_code =
            "#version 330 core\n"
            "layout(location = 0) in vec3 aPos;"
            "layout(location = 1) in vec3 aColor;"
            "out vec3 Color;"
            "void main() {"
                "gl_Position = vec4(aPos, 1.0);"
                "Color = aColor;"
            "}";
        const char* f_shader_code = 
            "#version 330 core\n"
            "out vec4 FragColor;"
            "in vec3 Color;"
            "void main() {"
            "    FragColor = vec4(Color, 1.0);"
            "}";

        // compile shaders
        unsigned int vertex, fragment;

        // vertex Shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &v_shader_code, NULL);
        glCompileShader(vertex);

        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &f_shader_code, NULL);
        glCompileShader(fragment);

        // shader Program
        line_shader = glCreateProgram();
        glAttachShader(line_shader, vertex);
        glAttachShader(line_shader, fragment);
        glLinkProgram(line_shader);

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        return line_shader;
    }

    void drawLine(glm::vec2 p1, glm::vec2 p2)
    {
        // point 1
        line_data.push_back(p1.x);
        line_data.push_back(p1.y);
        line_data.push_back(0);

        // color
        line_data.push_back(line_color.r);
        line_data.push_back(line_color.g);
        line_data.push_back(line_color.b);

        // point 2
        line_data.push_back(p2.x);
        line_data.push_back(p2.y);
        line_data.push_back(0);

        // color
        line_data.push_back(line_color.r);
        line_data.push_back(line_color.g);
        line_data.push_back(line_color.b);
    }

    void drawLinesFlush()
    {
        static unsigned int vao, vbo;

        static bool created = false;
        if (!created)
        {
            created = true;

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, line_data.size() * sizeof(float),
                line_data.data(), GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                (void*)0);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                (void*)(3 * sizeof(float)));
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, line_data.size() * sizeof(float),
                line_data.data(), GL_DYNAMIC_DRAW);
        }

        glUseProgram(line_shader);

        // 6 floats make up a vertex (3 position 3 color)
        // divide by that to get number of vertices to draw
        int count = line_data.size() / 6;

        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, count);

        line_data.clear();
    }

    void setLineWidth(GLfloat width) {
        glLineWidth(width);
    }

}
#endif