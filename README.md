![Robot](https://github.com/user-attachments/assets/50947aaa-ea59-4dec-962b-da7ced22c091)

Wi-Fi connected mobile robot that autonomously follows a colored target while avoiding obstacles.

An ESP32-CAM streams live MJPEG video to a host computer, where a real-time computer vision pipeline (OpenCV + NumPy) processes each frame. Each frame is downsampled, thresholded in HSV (with hue wrapping), denoised using largest connected-component filtering, and then analyzed to compute a blob centroid. Movement commands are then sent over UDP to an ESP32, which runs obstacle detection using 4 time-of-flight LiDAR sensors, forwarding a drive command to an ESP8266 in command of two L298N motor controllers powering a four-motor mecanum wheel system. 
ESP32-CAM to stream to a host computer, where a computer vision pipeline (OpenCV + NumPy) processes each frame. 
<img width="1391" height="1175" alt="image" src="https://github.com/user-attachments/assets/f1997900-4330-4841-beb0-6c451c16011f" />
The target color is selected by running color_chasher_pc.py and adjusting the HSV thresholds, as shown above, to isolate the color of the target object.
