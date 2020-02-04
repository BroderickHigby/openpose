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

openpose_args = "build/examples/openpose/openpose.bin --model_pose BODY_25 --tracking 1 --render_pose 1 --display 0 --write_json output/".split()

def run():
	'''
	input: Video and json args 
	output: Keypoint tracking JSON 
	'''
	json_data = os.getenv("JSON_DATA", None)
	video_file = os,getenv("VIDEO_FILE", None)

	start = timer()
	json_data = json.loads(str(json_data))
	if video_file is not None: 
		video_arg = ("--video", video_file) 
		args = str(openpose_args.append(video_arg))
		popen = subprocess.Popen(args, stdout=subprocess.PIPE)
		popen.wait()
		output = popen.stdout.read()
		print(output)

	    time_elapsed = timedelta(seconds=timer()-start)
        print(f"*** Completed Reid. Time elapsed: {time_elapsed} ***")

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
	

