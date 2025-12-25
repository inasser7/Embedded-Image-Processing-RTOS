// image_bridge.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	// Capture one frame from the camera and save it to outPath.
	// Returns 0 on success, negative on error.
	int cam_capture_and_save(const char* outPath);

	// Processors: read inPath, write outPath. Return 0 on success, negative on error.
	int img_invert_process(const char* inPath, const char* outPath);
	int img_blur_process(const char* inPath, const char* outPath);
	int img_grayscale_process(const char* inPath, const char* outPath);
	int img_edge_process(const char* inPath, const char* outPath);

	// Show original + processed images (invert/blur/edge/gray) for a given index.
	// Uses Images/capture_%03d.png, etc. Returns 0 on success, negative on error.
	int img_show_pipeline(int idx);

#ifdef __cplusplus
}
#endif
