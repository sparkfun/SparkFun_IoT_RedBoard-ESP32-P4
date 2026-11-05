# Troubleshooting

## BMA456 Feature Sets

The BMA456 feature engine runs one Bosch configuration at a time. For step counting and activity use `myAccel.begin(sfeBMA456FeaturesH)`. For orientation, high-g/low-g (free fall) and significant motion, use `myAccel.begin(sfeBMA456FeaturesMM)`. Both feature sets include tap, any/no-motion and auto low power. `myAccel.begin(sfeBMA456FeaturesNone)` skips the features and runs the BMA456 as a plain accelerometer.

## Support

If you need technical assistance or more information on a product that is not working as you expected, we recommend heading on over to the [SparkFun Technical Assistance](https://www.sparkfun.com/technical_assistance) page for some initial troubleshooting.

If you need technical assistance and more information on a product that is not working as you expected, visit the [SparkFun Community](https://community.sparkfun.com). The community forums are monitored by the SparkFun Technical Support Team and the SparkFun community as a whole. If this is your first visit to our forum, you'll need to create a [Forum Account](https://forum.sparkfun.com/ucp.php?mode=register) to post questions.

        