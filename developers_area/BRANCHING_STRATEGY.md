```text
                         TAGGED WITH RELEASE BRANCH NAME
                                      |
                                      V
main -----------------------------------------------------------------
      |                                               ^          |
      V                                               |          V
RELEASE_X_Y ------------------------------------------            RELEASE_X+1_Y+1 (optional)
           |    ^            |    ^         |    ^
           V    |            V    |         V    |
        feature1           feature2      feature3 (and so on)



- X and Y define major and minor changes.
- Remember to pull your changes at frequent intervals.