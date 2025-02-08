# Assetto Corsa image capture

The Assetto Corsa image capture is a python library using C code to grab frames published by the Custom Shader Pack. Setup of Assetto Corsa and the Custom Shader Pack (CSP) can be found on their [GitHub repository for the OBS plugin](https://github.com/ac-custom-shaders-patch/acc-obs-plugin) which was taken as implementation reference. Note, that in order for the OBS output to be available in the CSP, a patch for a version upgrade must be installed, as it requires CSP v0.1.80 or newer.
The option for OBS output must be enabled in the CSP.

## Setting up the library

In order to use the library, building and installing it with `pip install .` in this folder is enough. It can then be used as regular python library and can be imported with `import accapture`.

An example where the frames are captured and saved can be found in the [example.py](example.py). This example however, as problem with saving the image at the same speed as they are generated. In reality this could only be really overcome, by saving it as a video like OBS is doing and extract the images later on.

## Using the library

Calling get_frame() returns he current image. When called the first time, it will also initialize the capture.\\
Calling shutdown() can be used, when capture is not needed anymore.

## Functionality Problems

It is not exactly clear on how to synchronize the textures. This is why calling `get_frame(wait_for_new_frame=True)` will not work as intended.

The problem is that CSP is using most likely a triple buffer for the frame data and it. The field `needs_data` seems to indicate to the CSP that it should now render frames to the shared texture. When setting it to 3 it will be decreased by CSP and a new texture will be roughly available at that moment, but you will first get old images already in the buffer for the first two frames.
The OBS plugin manages to get a delay of only one frame (in my observations), so this could be debuged more to see what it is doing. See the section below for this.

## Alternatives

You could alternatively use OBS to capture the images for training purpose, but I did not think that this approach is feasable, as the image data needs to be delivered to the AI driver in realtime when it is actually driving. If for some reason that approach would still be reasonable, then here is a quick guide on how to do this with OBS that I tried out and worked.

OBS is making good use of video compression and manages to stream the data in realtime onto the disk because of the reduced data size. This however adds the need for after processing. With ffmpeg you can then generate a textfile that contains the time offset of every frame in relation to the start from the video. You can also extract an image sequence out of the video data with ffmpeg. What is still needed is the exact time stamp of the start of the video. I wrote a small python script that calld ffmpeg with the right parameters to perform these operations (ChatGPT knows really well how to configure ffmpeg) and renames all the images, based on the start time and the offset for each frame.

The start time was a little more dificult to generate. I cloned the OBS repository and added the code of the plugin into the plugins folder. OBS is structured very well into different "modules" for its plugin system. I searched for all registrations of `obs_output_info` where I added breakpoints into the start function and observed which one triggered on starting the stream inside the OBS GUI. You can also play arround with different outputs in the settings. On the right one, I added a small code snipet that gets the time and writes it into a file.

This usage is a bit complicated and I do not think that it should be used to train the AI, as the method should be the same as when the AI is then later using live image data to drive the car.

## Future work

In order to improve the performance, another python image library like OpenCv can be used, which would reduce the amount of allocations needed in order to get the data into python and also has more performant image operations.

If you want to get really good performance, do not use python and stay in the C code with all of the data, but that might be difficult for the AI stuff.

Figure out what the CSP is really doing with the frame buffering. But this is very difficult as it is not open source.

## Conclusion

I have put quite some time into reverse engeneering the texture handling and managed to make it working. I wrapped it in python as the AI driver code is using it, so it could also get images. Some aspects of synchronizing the textures are still not completely worked out. If you want to philisophize about use cases for this code, how it is working, or how CSP is doing things, write a short mail to fussenegger@students.tugraz.at and we could talk about this over a coffee.
