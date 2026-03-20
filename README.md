# Opti Voxel


# Development 

The easiest way to develop Opti Voxel is to setup an IDE.
The main tutorial from the documentation is nice [Configuring an IDE/Visual Studio Code](https://docs.godotengine.org/en/4.3/contributing/development/configuring_an_ide/visual_studio_code.html)


## VS code

For the user of VS Code a quick summary below :

Following these instructions you should be able to have a working debugging suite for the extension. It can be noted that the debugger won't work for the process spawned by the editor (if you choose to use the `Launch Project (Editor)` path in the `launch.json` ) so this can only be use to debug the actual editor. If you want to debug the test game you should use the `Launch Project` path.

Create in `.vscode` folder the following file :

`launch.json` :

Don't forget to modify actual project `--path` and `program` path

```
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
        "name": "Launch Project",
        "type": "cppvsdbg",
        "request": "launch",
        // Change the path below to the location of your Godot executable.
        "program": "${workspaceFolder}/../Godot_v4.6.1-stable_win64.exe/Godot_v4.6.1-stable_win64.exe",
        // Change the arguments below for the project you want to test with.
        "args": ["--path", "../voxel-lib/project" ],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "console": "internalConsole",
        "visualizerFile": "${workspaceFolder}/platform/windows/godot.natvis",
        "preLaunchTask": "build"
        },
        {
        "name": "Launch Project (Editor)",
        "type": "cppvsdbg",
        "request": "launch",
        // Change the path below to the location of your Godot executable.
        "program": "${workspaceFolder}/../Godot_v4.6.1-stable_win64.exe/Godot_v4.6.1-stable_win64.exe",
        // Change the arguments below for the project you want to test with.
        "args": ["--editor", "--path", "../voxel-lib/project" ],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "console": "internalConsole",
        "visualizerFile": "${workspaceFolder}/platform/windows/godot.natvis",
        "preLaunchTask": "build"
        }
    ]
}
```

`tasks.json` :

```
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        {
        "label": "build",
        "group": "build",
        "type": "shell",
        "command": "scons",
        "args": [
            // enable for debugging with breakpoints
            "dev_build=yes",
        ],
        "problemMatcher": "$msCompile"
        }
    ]
}
```

