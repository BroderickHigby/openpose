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
    video_file = 'video/cardiB.mp4'
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

    # TODO: Remove dumps
    pose_data = json.dumps(output)
    print(pose_data)
    if JOB_ID is not None:
        print(f"Save output to Firebase at job_id: {JOB_ID}")

        pose_data = json.dumps(output)

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
	

