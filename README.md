Level 13 – File Download Client

How to compile the program:
I compiled it using this command:

gcc download.c md5.c makehash.c -o download


This creates the download executable.

Extra functionality I implemented:

The program checks if a file already exists and asks before overwriting it.

I added a “download all files” option.

I added a simple progress bar during downloads so you can see the percentage as it downloads.

What I’m proud of:
I’m proud that I got the full download flow working correctly, especially for binary files.
Getting the chunked reading/writing and the remaining-bytes loop right took some trial and error,
so seeing JPEGs and text files download fully and open correctly felt really good.

Notes when running the program:

Make sure to run the LIST option first if you want to use the “download all files” feature.

The program should work normally on both servers.

If you cancel a download by saying no to overwriting, it skips it and keeps going.

Everything compiles without warnings on my end.
