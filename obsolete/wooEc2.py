"""
!!!! WIP !!!!

TODO: adjust batch launcher to submit jobs to the queue.

Consumer of Amazon AWS SQS messages which start Woo jobs on the node.

Inspired by https://github.com/danilop/SampleBatchProcessing/blob/master/GetJobs.py
"""


def main():
    import argparse
    par=argparse.ArgumentParser(prog=os.path.basename(sys.argv[0]),description='Woo-EC2: Woo launcher from Amazon AWS SQS queue.')
    #par.add_argument('-C','--chdir',dest='chdir',default='/tmp',help='Working directory')
    # AWS-region is obtainable from instance metadata
    # but queue must be passed through user-data
    par.add_argument('--sqs-queue',dest='sqsQueue',help='Name of SQS queue.')
    par.add_argument('--s3-bucket',dest='s3bucket',help='Name of S3 bucket for input/output data.')



    # https://stackoverflow.com/a/25199759/761090
    import boto.utils
    imeta=boto.utils.get_instance_metadata(timeout=.5,num_retries=1)
    region=imeta['placement']['availability-zone'][:-1]

    import boto.s3, boto.sqs
    s3=boto.s3.connect_to_region(region)
    sqs=bobo.sqs.connect_to_region(region)
    sqsQueue=sqs.lookup(args.sqsQueue)
    while True:
        print('Getting SQS messages...')
        messages=sqsQueue.get_messages(wait_time_seconds=20)
        for m in messages:
            # if not message about starting a job, ignore
            # delete msg from the queue
            # create tempdir for the job
            # copy input files from s3 into local input dir (?)
            # create output dir
            # run simulation (TODO: default executable name=?)
            ## what to do while running? progress can be sent as SQS msg, but not plots (>256kB)
            ## or open XMLRPC port to internet and announce the outer IP via SQS msg?
            ## idea: react to queries sent via SQS (progress etc); later also upload plot+log to s3 on-demand
            # pack output dir and copy back to s3
            # send SQS msg about job finished
            



