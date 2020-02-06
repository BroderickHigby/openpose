from tqdm import tqdm
import os
import json
import subprocess
import firebase_admin
from firebase_admin import credentials
from firebase_admin import firestore
from datetime import timedelta
from timeit import default_timer as timer

cred = credentials.Certificate("serviceAccount.json")
firebase_admin.initialize_app(cred)
client = firestore.client()

DB_BASE = os.getenv("DB_BASE", "jobs")
COMPANY_ID = os.getenv("COMPANY_ID", "goVertical")
JOB_ID = os.getenv("JOB_ID", None)


def run():
    '''
    input: Video and json args 
    output: Keypoint tracking JSON 
    '''
    video_file = os.getenv("VIDEO_FILE", None)
#    video_file = os.rename(video_file, 'video.mp4') 
    video_file = 'video/video.mp4'
    openpose_args = "build/examples/openpose/openpose.bin --model_pose BODY_25 --tracking 1 --render_pose 0 --display 0 --write_json output/ --video "
    start = timer()
    if video_file is not None: 
        # video_arg = ("--video", video_file) 
       # print("Video Args: " + str(video_arg))
       # openpose_args += str(video_arg)
        openpose_args += video_file
        print("Openpose args: " + str(openpose_args))
        args = openpose_args.split()
        print("split args: " + str(args))
        popen = subprocess.Popen(args, stdout=subprocess.PIPE)
        popen.wait()
        output = popen.stdout.read()
        print(output)

    time_elapsed = timedelta(seconds=timer()-start)
    print(f"*** Completed Pose Estimation. Time elapsed: {time_elapsed} ***")
    if JOB_ID is not None:
        print(f"Save output to Firebase at job_id: {JOB_ID}")
        
        staf_data = {}
        pose_json_dir = './output'
        staf_files = os.listdir(pose_json_dir)
        staf_files_dic = {}
        frame_numbers = [[int(i[6:18]), i] for i in staf_files if i[0] != '.']
        for i in frame_numbers:
            staf_files_dic[i[0]] = i[1]
            for frame in tqdm(staf_files_dic):
                file_name = staf_files_dic[frame]
                with open(os.path.join(pose_json_dir ,file_name)) as f:
                    staf_data = json.load(f)
                    pose_data = json.dumps(staf_data)


        for index, item in enumerate(pose_data):
            (
                client.collection(DB_BASE)
                .document(COMPANY_ID)
                .collection("jobData")
                .document(JOB_ID)
                .collection("poseServerData")
                .document(str(index))
                .set({"poseServerData": item})
            )
    else:
        print("No video file specified")


if __name__ == "__main__":
    run()
	

