# Need to discard STDERR so get path to NULL device
win32 {
    NULL_DEVICE = NUL # Windows doesn't have /dev/null but has NUL
} else {
    NULL_DEVICE = /dev/null
}

    ERASE_COMMAND = rm {myfunctions.cpp, myfunctions.h}
win32|win64{
    ERASE_COMMAND = del myfunctions.cpp, myfunctions.h
}

system($$ERASE_COMMAND 2> $$NULL_DEVICE)

win32|win64{
    system(powershell -Command "(New-Object Net.WebClient).DownloadFile('https://raw.githubusercontent.com/DrSmyrke/QT-Libs/master/myfunctions.cpp', 'myfunctions.cpp')")
    system(powershell -Command "(New-Object Net.WebClient).DownloadFile('https://raw.githubusercontent.com/DrSmyrke/QT-Libs/master/myfunctions.h', 'myfunctions.h')")
}else{
    system(curl --proxy-negotiate -u: https://raw.githubusercontent.com/DrSmyrke/QT-Libs/master/myfunctions.cpp > myfunctions.cpp)
    system(curl --proxy-negotiate -u: https://raw.githubusercontent.com/DrSmyrke/QT-Libs/master/myfunctions.h > myfunctions.h)
}
