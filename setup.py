from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='robotick',
    version='0.1.0',
    description='A modular Python framework for robotics and control systems, supporting real-time workloads and simulation.',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='Paul Connor',
    author_email='paulwconnor-ai@outlook.com',
    url='https://github.com/paulwconnor-ai/robotick',
    packages=find_packages(where="python"),
    package_dir={"": "python"},
    install_requires=[
        'numpy',
        'paho-mqtt',
        'PyYAML',
        'rich'
    ],
    classifiers=[
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: OS Independent',
        'Topic :: Scientific/Engineering :: Robotics',
        'Intended Audience :: Developers',
    ],
    python_requires='>=3.7',
)
