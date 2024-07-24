# Current Loop Flow

1. Wait for GPU to complete render for current frame.
2. Get an image from the swapchain so we can render to it.
3. Set it in a writeable layout
4. Clear its color
5. transition it to a presentable layout