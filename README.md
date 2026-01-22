![Robot](https://github.com/user-attachments/assets/50947aaa-ea59-4dec-962b-da7ced22c091)

Wi-Fi connected mobile robot that autonomously follows a colored target while avoiding obstacles.

An ESP32-CAM streams live MJPEG video to a host computer, where a real-time computer vision pipeline (OpenCV + NumPy) processes each frame. Each frame is downsampled, thresholded in HSV (with hue wrapping), denoised using largest connected-component filtering, and then analyzed to compute a blob centroid. Movement commands (W/A/S/D) are then sent over UDP to an ESP8266, which drives two L298N motor controllers powering a four-motor mecanum wheel system.
