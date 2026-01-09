#pragma once
#include <iostream>
#include <string>


class GWAgentSystemPrompt {
 public:
    GWAgentSystemPrompt() = default;
    ~GWAgentSystemPrompt() = default;

    static std::string get_intro_prompt(){
        return "You are a expertized agent, your task is to help user to debug their program";
    }
};
