# FiliPusher
live encoder, video h.264, audio AAC, rtmp push. 在线真人视讯，在线棋牌游戏专用推流端软件。

# 开源版本：单摄像头
	1.可以推一路摄像头和一路audio，固定：H.264+AAC。
	2.可以在流上打timestamp，精准到ms。 
	3.可以同时推三个不同流服地址。
	4.可以录像到NAS存储。
	5.可以订阅接收录像的分段信号(record start/stop)，mqtt订阅协议。
	6.H.264 encoder参数可以选：baseline/main profile, GOP。
	7.内置webserver，不支持video回显。
	8.推流协议：rtmp/webrtc。

# 收费版本：双摄像头和H.265。
	1.支持2路摄像头及其混流。
	2.每路摄像头可以分别出3路流，可以分别进行crop，可以分别配置是否带audio。
	3.支持一路混流，可以设置2路摄像头的相对位置。
	4.可以分别控制在流上打timestamp，精准到ms。 
	5.可以同时推三个不同的流服地址。
	6.可以录像到NAS存储。
	7.可以订阅接收录像的分段信号(record start/stop)，mqtt订阅协议。
	8.video encoder参数可以选：H.264/H.265, baseline/main profile, GOP。
	9.支持GPU优化以减轻CPU负担。
	10.内置webserver，不支持video回显。
	11.推流协议：rtmp/webrtc。
	后续分支出主播版本：支持桌面和弹幕。

# contact:
	add qq Group FiliPusher(688418930).
	email: gregtham91@gmail.com
