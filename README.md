## Universal Updater wow :D

-----------------------------------------

# How to Use

It's simple, it just has a few args that you need to pass which are:
-link
-pid
-absolute dir
-sha256 (no other sha type)
-launch (optional)

Which lines up to this:
* C:\OwOpdater\OwOpdater.exe -link https://yourlinkhere -absolute-dir C:\\Users\\Femboys\\tests -sha-256 sha256assdfasd -pid 1234

And with launch arg
* C:\OwOpdater\OwOpdater.exe -link https://yourlinkhere -absolute-dir C:\\Users\\Femboys\\tests -sha-256 sha256assdfasd -pid 1234 -launch Lunox.exe

* This is CLI only

-----------------------------------------

# How it works 

1. Downloads the executable or zip file from the -link you gave

2. Kills the process ID (-pid) and any child processes it spawned

3. Verifies if the file matches the SHA256 u gave

4. Extracts the contents if it's a zip file 

5. Swaps the old files with the new ones in -absolute-dir (clears target folder first)

6. Launches the executable specified in -launch (if u passed that arg, relative to the dir u passed earlier)

-----------------------------------------

Yes I will update this from time to time when im free or special ocassions 

Bye bye!!
