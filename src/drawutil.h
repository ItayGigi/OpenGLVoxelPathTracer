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
            "layout(location = 0) in vec2 aPos;"
            "layout(location = 1) in vec3 aColor;"
            "layout(location = 2) in vec3 aViewPos1;"
            "layout(location = 3) in vec3 aViewPos2;"

            "out vec3 Color;"
            "out vec2 ScreenPos;"
            "flat out vec3 View1;"
            "flat out vec3 View2;"

            "void main() {"
            "   gl_Position = vec4(aPos, 0.0, 1.0);"
            "   Color = aColor;"
            "   ScreenPos = aPos;"
            "   View1 = aViewPos1;"
            "   View2 = aViewPos2;"
            "}";
        const char* f_shader_code = 
            "#version 330 core\n"
            "uniform sampler2D DepthTex;"
            "uniform ivec2 Resolution;"

            "out vec4 FragColor;"

            "in vec3 Color;"
            "in vec2 ScreenPos;"
            "flat in vec3 View1;"
            "flat in vec3 View2;"

            "void main() {"
            "   FragColor = vec4(Color, 0.);"

            "   if (View1 == View2) return;"

            "   vec2 TexCoords = ScreenPos * 0.5 + 0.5;"
            "   float depthSamples[] = float[]( "
            "       texture(DepthTex, ScreenPos * 0.5 + 0.5).r, "
            "       texture(DepthTex, TexCoords + 1./Resolution).r, "
            "       texture(DepthTex, TexCoords - 1./Resolution).r, "
            "       texture(DepthTex, TexCoords + ivec2(-1, 1)*1./Resolution).r, "
            "       texture(DepthTex, TexCoords + ivec2(1, -1)*1./Resolution).r );"

            "   float t = (1.5 * View1.y - ScreenPos.y * View1.z) / (ScreenPos.y * (View2.z - View1.z) - 1.5 * (View2.y - View1.y));"
            "   vec3 view = mix(View1, View2, t);"

            "   for (int i = 0; i < 5; i++)"
            "       if (depthSamples[i] < 0. || depthSamples[i] > length(view) - 0.005) return;"

            "   discard;"
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

    void passDepthTexture(unsigned int texture, unsigned int slot) {
        glUseProgram(line_shader);

        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(line_shader, "DepthTex"), slot);
    }

    void passResolution(int width, int height) {
        glUseProgram(line_shader);

        glUniform2i(glGetUniformLocation(line_shader, "Resolution"), width, height);
    }

    void drawLineDepth(glm::vec2 p1, glm::vec3 view1, glm::vec2 p2, glm::vec3 view2)
    {
        // point 1
        line_data.push_back(p1.x);
        line_data.push_back(p1.y);

        // color
        line_data.push_back(line_color.r);
        line_data.push_back(line_color.g);
        line_data.push_back(line_color.b);

        // view pos 1
        line_data.push_back(view1.x);
        line_data.push_back(view1.y);
        line_data.push_back(view1.z);

        // view pos 2
        line_data.push_back(view2.x);
        line_data.push_back(view2.y);
        line_data.push_back(view2.z);

        // point 2
        line_data.push_back(p2.x);
        line_data.push_back(p2.y);

        // color
        line_data.push_back(line_color.r);
        line_data.push_back(line_color.g);
        line_data.push_back(line_color.b);

        // view pos 1
        line_data.push_back(view1.x);
        line_data.push_back(view1.y);
        line_data.push_back(view1.z);

        // view pos 2
        line_data.push_back(view2.x);
        line_data.push_back(view2.y);
        line_data.push_back(view2.z);
    }

    void drawLine(glm::vec2 p1, glm::vec2 p2)
    {
        drawLineDepth(p1, glm::vec3(0.f), p2, glm::vec3(0.f));
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
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                (void*)0);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                (void*)(2 * sizeof(float)));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                (void*)(5 * sizeof(float)));

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                (void*)(8 * sizeof(float)));
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