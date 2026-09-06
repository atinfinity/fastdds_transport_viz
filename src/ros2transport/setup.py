from setuptools import find_packages
from setuptools import setup

package_name = 'ros2transport'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['ros2cli'],
    zip_safe=True,
    author='atinfinity',
    author_email='dandelion1124@gmail.com',
    maintainer='atinfinity',
    maintainer_email='dandelion1124@gmail.com',
    description='ros2 transport: which Fast DDS transport each ROS 2 topic uses.',
    license='Apache-2.0',
    tests_require=['pytest'],
    extras_require={'test': ['pytest']},   # setuptools on Python 3.14 drops tests_require; colcon looks here
    entry_points={
        'ros2cli.command': [
            'transport = ros2transport.command.transport:TransportCommand',
        ],
        'ros2cli.extension_point': [
            'ros2transport.verb = ros2transport.verb:VerbExtension',
        ],
        'ros2transport.verb': [
            'list = ros2transport.verb.list:ListVerb',
            'codes = ros2transport.verb.codes:CodesVerb',
        ],
    },
)
