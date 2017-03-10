## A NDN Approach to Energy Efficiency

### Prerequisites

Some patches required for this application have not yet been merged into the
master version of RIOT and CCN-lite, hence, you need to use particular branches
from my forks. This branch of RIOT is already configured correctly:
 https://github.com/OlegHahm/RIOT/tree/ccnl_caching

Also, make sure that you adapt the path to RIOT in this application's Makefile
correctly.

### Usage

You can configure most of the parameters in `dow.h`. The configured settings
will be displayed upon start.

To start the application, use the shell command `start`. It will automatically
start CCN-lite, using the default network interface.

Further additional shell commands are `c` and `stats`.
`c` will print the current content of the node's content store (if it is
active), while `stats` will print CCN-lite packet statistics.
